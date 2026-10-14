#include "ProjectJoinList.h"

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
    QVector<ProjectJoinGroup> groups;
    groups.reserve(snapshot.equateLinks.size());
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
                group.pairs.append(ProjectJoinPair{joinSide(snapshot, first, projection),
                                                   joinSide(snapshot, second, projection)});
            }
        }
        groups.append(group);
    }
    return groups;
}
}
