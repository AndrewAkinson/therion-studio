#include "ProjectJoinList.h"

#include "ProjectJoinRules.h"

#include <QHash>
#include <QPair>
#include <QSet>

#include <algorithm>
#include <tuple>

namespace TherionStudio
{
namespace
{
std::tuple<int, qreal, QString> projectionSortKey(const QString &projection)
{
    if (projection == QStringLiteral("plan")) {
        return {0, 0.0, projection};
    }
    if (projection == QStringLiteral("extended")) {
        return {1, 0.0, projection};
    }
    if (projection.startsWith(QStringLiteral("elevation"))) {
        bool ok = false;
        const qreal angle = projection.section(QLatin1Char(' '), 1, 1).toDouble(&ok);
        return {2, ok ? angle : -1.0, projection};
    }
    return {3, 0.0, projection};
}

ProjectJoinConnection connectionForJoin(const ProjectJoinCommand &join)
{
    if (join.mode == ProjectJoinMode::PointsAndLines) {
        return ProjectJoinConnection::PointLineJoin;
    }
    return join.count > 0 ? ProjectJoinConnection::CountedScrapJoin : ProjectJoinConnection::AutomaticScrapJoin;
}

ProjectJoinConnection combinedConnection(const QVector<ProjectJoinCommand> &joins)
{
    ProjectJoinConnection connection = ProjectJoinConnection::None;
    for (const ProjectJoinCommand &join : joins) {
        const ProjectJoinConnection joinConnection = connectionForJoin(join);
        if (connection != ProjectJoinConnection::None && connection != joinConnection) {
            return ProjectJoinConnection::MixedJoins;
        }
        connection = joinConnection;
    }
    return connection;
}

// Survey namespaces of a join's resolved items whose scraps are drawn in `projection`.
QSet<QString> joinNamespacesInProjection(const ProjectIndexSnapshot &snapshot,
                                         const ProjectJoinCommand &join,
                                         const QString &projection)
{
    QSet<QString> namespaces;
    for (const ProjectJoinItem &item : join.items) {
        if (item.resolved && snapshot.scrapProjectionByObjectId.value(item.scrapObjectId) == projection) {
            namespaces.insert(item.namespacePath.toLower());
        }
    }
    return namespaces;
}

QString namespacePairKey(QString first, QString second)
{
    first = first.toLower();
    second = second.toLower();
    return first < second ? first + QLatin1Char('|') + second : second + QLatin1Char('|') + first;
}

// A key for every pair of distinct namespaces in `namespaces`.
QStringList surveyPairKeys(const QStringList &namespaces)
{
    QStringList keys;
    for (int first = 0; first < namespaces.size(); ++first) {
        for (int second = first + 1; second < namespaces.size(); ++second) {
            if (namespaces.at(first).compare(namespaces.at(second), Qt::CaseInsensitive) != 0) {
                keys.append(namespacePairKey(namespaces.at(first), namespaces.at(second)));
            }
        }
    }
    return keys;
}

ProjectJoinSide joinSide(const ProjectIndexSnapshot &snapshot,
                         const ProjectEquateStation &station,
                         const QString &projection)
{
    ProjectJoinSide side;
    side.station = station;
    if (!station.resolved) {
        side.status = ProjectJoinSideStatus::Unresolved;
        return side;
    }
    const QVector<ProjectStationDrawingPoint> points =
        snapshot.stationDrawingPointsByStationKey.value(station.stationKey);
    if (points.isEmpty()) {
        side.status = ProjectJoinSideStatus::NotDrawn;
        return side;
    }
    for (const ProjectStationDrawingPoint &point : points) {
        if (point.projection == projection) {
            side.status = ProjectJoinSideStatus::Drawn;
            side.drawing = point;
            return side;
        }
    }
    side.status = ProjectJoinSideStatus::NotDrawnInProjection;
    return side;
}
}

QStringList projectJoinProjections(const ProjectIndexSnapshot &snapshot)
{
    QSet<QString> projections;
    for (const QVector<ProjectStationDrawingPoint> &points : snapshot.stationDrawingPointsByStationKey) {
        for (const ProjectStationDrawingPoint &point : points) {
            projections.insert(point.projection);
        }
    }
    QStringList sorted(projections.cbegin(), projections.cend());
    std::sort(sorted.begin(), sorted.end(), [](const QString &left, const QString &right) {
        return projectionSortKey(left) < projectionSortKey(right);
    });
    return sorted;
}

QVector<ProjectJoinGroup> projectJoinGroups(const ProjectIndexSnapshot &snapshot, const QString &projection)
{
    // Cross-survey joins drawn in this projection, by the survey pairs they connect.
    QHash<QString, QVector<ProjectJoinCommand>> joinsBySurveyPair;
    QVector<QPair<ProjectJoinCommand, QSet<QString>>> crossSurveyJoins;
    for (const ProjectJoinCommand &join : snapshot.joinCommands) {
        const QSet<QString> namespaces = joinNamespacesInProjection(snapshot, join, projection);
        if (namespaces.size() < 2) {
            continue;
        }
        crossSurveyJoins.append({join, namespaces});
        for (const QString &pairKey : surveyPairKeys(QStringList(namespaces.cbegin(), namespaces.cend()))) {
            joinsBySurveyPair[pairKey].append(join);
        }
    }

    // Survey pairs linked by any equate or nosurvey leg, in any projection.
    QSet<QString> linkedSurveyPairs;
    for (const ProjectEquateLink &link : snapshot.equateLinks) {
        QStringList stationNamespaces;
        for (const ProjectEquateStation &station : link.stations) {
            stationNamespaces.append(station.namespacePath);
        }
        for (const QString &pairKey : surveyPairKeys(stationNamespaces)) {
            linkedSurveyPairs.insert(pairKey);
        }
    }

    QVector<ProjectJoinGroup> groups;
    groups.reserve(snapshot.equateLinks.size() + crossSurveyJoins.size());
    for (const ProjectEquateLink &link : snapshot.equateLinks) {
        ProjectJoinGroup group;
        group.link = link;
        for (int firstIndex = 0; firstIndex < link.stations.size(); ++firstIndex) {
            for (int secondIndex = firstIndex + 1; secondIndex < link.stations.size(); ++secondIndex) {
                const ProjectEquateStation &first = link.stations.at(firstIndex);
                const ProjectEquateStation &second = link.stations.at(secondIndex);
                if (first.namespacePath.compare(second.namespacePath, Qt::CaseInsensitive) == 0) {
                    continue;
                }
                ProjectJoinPair pair{joinSide(snapshot, first, projection), joinSide(snapshot, second, projection)};
                pair.linkKind = link.kind;
                pair.joins = joinsBySurveyPair.value(namespacePairKey(first.namespacePath, second.namespacePath));
                pair.connection = combinedConnection(pair.joins);
                pair.ruleViolations = projectJoinRuleViolationsFor(snapshot.joinRuleViolations, pair.joins);
                group.pairs.append(pair);
            }
        }
        groups.append(group);
    }

    QHash<QString, ProjectStructureEntry> entriesByObjectId;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        entriesByObjectId.insert(entry.objectId, entry);
    }
    for (const auto &[join, namespaces] : std::as_const(crossSurveyJoins)) {
        const QStringList pairKeys = surveyPairKeys(QStringList(namespaces.cbegin(), namespaces.cend()));
        if (std::any_of(pairKeys.cbegin(), pairKeys.cend(), [&linkedSurveyPairs](const QString &pairKey) {
                return linkedSurveyPairs.contains(pairKey);
            })) {
            continue;
        }
        ProjectJoinGroup group;
        group.joinOnly = join;
        group.joinOnlyConnection = connectionForJoin(join);
        group.joinOnlyRuleViolations = projectJoinRuleViolationsFor(snapshot.joinRuleViolations, {join});
        QSet<QString> seenScraps;
        for (const ProjectJoinItem &item : join.items) {
            if (item.resolved && !seenScraps.contains(item.scrapObjectId)) {
                seenScraps.insert(item.scrapObjectId);
                group.joinOnlyScraps.append(entriesByObjectId.value(item.scrapObjectId));
            }
        }
        groups.append(group);
    }
    return groups;
}
}
