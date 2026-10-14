#include "../../src/core/ProjectJoinList.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectEquateStation station(const QString &reference, const QString &namespacePath, bool resolved = true)
{
    return ProjectEquateStation{reference, reference.toLower() + QLatin1Char('|') + namespacePath, namespacePath, resolved};
}

ProjectStationDrawingPoint drawing(const ProjectEquateStation &station, const QString &file, const QString &projection)
{
    return ProjectStationDrawingPoint{station.stationKey, file, 7, QStringLiteral("s"), QStringLiteral("survey"), projection};
}
}

class ProjectJoinListTest final : public QObject
{
    Q_OBJECT

private slots:
    void ordersProjections()
    {
        ProjectIndexSnapshot snapshot;
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        for (const QString &projection : {QStringLiteral("elevation 315"), QStringLiteral("none-like"),
                                          QStringLiteral("elevation 90"), QStringLiteral("extended"),
                                          QStringLiteral("plan"), QStringLiteral("elevation")}) {
            snapshot.stationDrawingPointsByStationKey[a.stationKey].append(drawing(a, QStringLiteral("a.th2"), projection));
        }
        QCOMPARE(projectJoinProjections(snapshot),
                 QStringList({QStringLiteral("plan"), QStringLiteral("extended"), QStringLiteral("elevation"),
                              QStringLiteral("elevation 90"), QStringLiteral("elevation 315"),
                              QStringLiteral("none-like")}));
    }

    void buildsPairsWithSideStatus()
    {
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        const ProjectEquateStation b = station(QStringLiteral("2@b"), QStringLiteral("b"));
        const ProjectEquateStation sameNamespaceAsA = station(QStringLiteral("3@a"), QStringLiteral("a"));
        const ProjectEquateStation c = station(QStringLiteral("4@c"), QStringLiteral("c"));
        const ProjectEquateStation missing = station(QStringLiteral("5@d"), QStringLiteral("d"), false);

        ProjectIndexSnapshot snapshot;
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 3, {a, b, sameNamespaceAsA, c}});
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 4, {b, missing}});
        snapshot.stationDrawingPointsByStationKey[a.stationKey] = {drawing(a, QStringLiteral("a-ee.th2"), QStringLiteral("extended")),
                                                                   drawing(a, QStringLiteral("a.th2"), QStringLiteral("plan"))};
        snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("b.th2"), QStringLiteral("plan"))};
        snapshot.stationDrawingPointsByStationKey[c.stationKey] = {drawing(c, QStringLiteral("c.th2"), QStringLiteral("extended"))};

        const QVector<ProjectJoinGroup> groups = projectJoinGroups(snapshot, QStringLiteral("plan"));
        QCOMPARE(groups.size(), 2);

        // a-b, a-c, b-3@a, b-c, 3@a-c; a and 3@a share a namespace so they do not pair.
        const QVector<ProjectJoinPair> &pairs = groups.at(0).pairs;
        QCOMPARE(pairs.size(), 5);
        QVERIFY(pairs.at(0).isAvailable());
        QCOMPARE(pairs.at(0).first.drawing->sourceFile, QStringLiteral("a.th2"));
        QCOMPARE(pairs.at(0).second.drawing->sourceFile, QStringLiteral("b.th2"));
        QCOMPARE(pairs.at(1).second.status, ProjectJoinSideStatus::NotDrawnInProjection);
        QVERIFY(!pairs.at(1).isAvailable());
        QCOMPARE(pairs.at(2).second.status, ProjectJoinSideStatus::NotDrawn);

        QCOMPARE(groups.at(1).pairs.size(), 1);
        QCOMPARE(groups.at(1).pairs.at(0).second.status, ProjectJoinSideStatus::Unresolved);
    }
};

int runProjectJoinListTest(int argc, char **argv)
{
    ProjectJoinListTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectJoinListTest.moc"
