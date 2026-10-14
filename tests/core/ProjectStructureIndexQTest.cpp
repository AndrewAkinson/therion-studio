#include "../../src/core/ProjectEquateLinkScanner.h"
#include "../../src/core/ProjectStructureIndex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QByteArray encodedContents = contents.toUtf8();
    return file.write(encodedContents) == encodedContents.size();
}
}

class ProjectStructureIndexQTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesScrapStationNamePrefixAndSuffix()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(projectDir.mkpath(QStringLiteral("maps")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/map.th2\n"
                                  "  centerline\n"
                                  "    data normal from to length compass clino\n"
                                  "    pre1suf pre2suf 1 0 0\n"
                                  "    pre3 pre4 1 0 0\n"
                                  "    1 1 1 0 0\n"
                                  "  endcenterline\n"
                                  "endsurvey cave\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap s -station-names pre suf\n"
                                  "point 0 0 station -name 1\n"
                                  "point 1 1 station -name 2\n"
                                  "point 2 2 station -name missing\n"
                                  "endscrap\n"
                                  "scrap s2 -station-names pre []\n"
                                  "point 3 3 station -name 3\n"
                                  "endscrap\n"
                                  "scrap s3 -station-names \"\" @cave\n"
                                  "point 4 4 station -name 1@cave\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList unknownStationReferences;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownStationReference) {
                unknownStationReferences.append(diagnostic.referencedName);
            }
        }

        QVERIFY(!unknownStationReferences.contains(QStringLiteral("1")));
        QVERIFY(!unknownStationReferences.contains(QStringLiteral("2")));
        QVERIFY(!unknownStationReferences.contains(QStringLiteral("3")));
        QCOMPARE(unknownStationReferences.count(QStringLiteral("missing")), 1);
        QCOMPARE(unknownStationReferences.count(QStringLiteral("1@cave")), 1);
    }

    void normalizesScrapProjectionNames()
    {
        QCOMPARE(normalizedScrapProjection(QString()), QStringLiteral("plan"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("Plan")), QStringLiteral("plan"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("[elevation  315]")), QStringLiteral("elevation 315"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("[elevation 90 deg]")), QStringLiteral("elevation 90"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("[elevation 100 grad]")), QStringLiteral("elevation 100 grad"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("elevation")), QStringLiteral("elevation"));
    }

    void indexesCrossSurveyEquateLinksAndStationDrawings()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        const QString centreline = QStringLiteral(
            "  centreline\n"
            "    data normal from to length compass clino\n"
            "    %1 %2 1 0 0\n"
            "%3"
            "  endcentreline\n");
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral("survey top\n"
                                             "survey a\n"
                                             "  input a.th2\n")
                                  + centreline.arg(QStringLiteral("1"), QStringLiteral("2"),
                                                   QStringLiteral("    equate 1 2\n"))
                                  + QStringLiteral("endsurvey a\n"
                                                   "survey b\n"
                                                   "  input b.th2\n")
                                  + centreline.arg(QStringLiteral("1"), QStringLiteral("2"), QString())
                                  + QStringLiteral("endsurvey b\n"
                                                   "survey e -namespace off\n"
                                                   "  input e.th2\n")
                                  + centreline.arg(QStringLiteral("9"), QStringLiteral("10"), QString())
                                  + QStringLiteral("endsurvey e\n"
                                                   "equate 2@a 1@b\n"
                                                   "#equate 1@a 2@b\n"
                                                   "equate 9 1@a 2@b\n"
                                                   "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("a.th2")),
                              QStringLiteral("scrap sa -projection plan\n"
                                             "point 0 0 station -name 2\n"
                                             "point 5 0 station -name 1\n"
                                             "endscrap\n"
                                             "scrap sae -projection [elevation 315]\n"
                                             "point 0 0 station -name 2\n"
                                             "endscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("b.th2")),
                              QStringLiteral("scrap sb\n"
                                             "point 0 0 station -name 1\n"
                                             "point 1 1 station -name 2\n"
                                             "endscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("e.th2")),
                              QStringLiteral("scrap se -projection plan\n"
                                             "point 0 0 station -name 9\n"
                                             "point 1 1 station -name 10\n"
                                             "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        // The same-survey and commented-out equates are not cross-survey links.
        QCOMPARE(snapshot.equateLinks.size(), 2);
        const ProjectEquateLink &pair = snapshot.equateLinks.at(0);
        QCOMPARE(pair.lineNumber, 24);
        QCOMPARE(pair.stations.size(), 2);
        QCOMPARE(pair.stations.at(0).reference, QStringLiteral("2@a"));
        QCOMPARE(pair.stations.at(0).namespacePath, QStringLiteral("a.top"));
        QVERIFY(pair.stations.at(0).resolved);
        QVERIFY(pair.stations.at(1).resolved);

        const ProjectEquateLink &group = snapshot.equateLinks.at(1);
        QCOMPARE(group.stations.size(), 3);
        QCOMPARE(group.stations.at(0).reference, QStringLiteral("9"));
        QCOMPARE(group.stations.at(0).namespacePath, QStringLiteral("top"));
        QVERIFY(group.stations.at(0).resolved);

        const QVector<ProjectStationDrawingPoint> stationTwoA =
            snapshot.stationDrawingPointsByStationKey.value(pair.stations.at(0).stationKey);
        QCOMPARE(stationTwoA.size(), 2);
        QCOMPARE(stationTwoA.at(0).scrapName, QStringLiteral("sa"));
        QCOMPARE(stationTwoA.at(0).surveyName, QStringLiteral("a"));
        QCOMPARE(stationTwoA.at(0).projection, QStringLiteral("plan"));
        QCOMPARE(stationTwoA.at(0).lineNumber, 2);
        QCOMPARE(stationTwoA.at(1).projection, QStringLiteral("elevation 315"));

        const QVector<ProjectStationDrawingPoint> stationOneB =
            snapshot.stationDrawingPointsByStationKey.value(pair.stations.at(1).stationKey);
        QCOMPARE(stationOneB.size(), 1);
        QCOMPARE(stationOneB.at(0).projection, QStringLiteral("plan"));

        const QVector<ProjectStationDrawingPoint> stationNine =
            snapshot.stationDrawingPointsByStationKey.value(group.stations.at(0).stationKey);
        QCOMPARE(stationNine.size(), 1);
        QCOMPARE(stationNine.at(0).surveyName, QStringLiteral("e"));

        // Station 10 is drawn but not equated, so it is not indexed as a join drawing point.
        for (const QVector<ProjectStationDrawingPoint> &points : snapshot.stationDrawingPointsByStationKey) {
            for (const ProjectStationDrawingPoint &point : points) {
                QVERIFY(!(point.scrapName == QStringLiteral("se") && point.lineNumber == 3));
            }
        }
    }
};

int runProjectStructureIndexQTest(int argc, char **argv)
{
    ProjectStructureIndexQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureIndexQTest.moc"
