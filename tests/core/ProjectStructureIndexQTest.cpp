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

    void reportsMapItemsThatDoNotExistWhateverTheirName()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(projectDir.mkpath(QStringLiteral("maps")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  survey branch\n"
                                  "  endsurvey branch\n"
                                  "  input maps/map.th2\n"
                                  "  map plan\n"
                                  "    plan1\n"
                                  "    PlanMissing2\n"
                                  "  endmap\n"
                                  "  map whole\n"
                                  "    plan\n"
                                  "    WholeMissing\n"
                                  "  endmap\n"
                                  "  map bySurvey\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map suffixed\n"
                                  "    missing.s\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap plan1\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList unknownMapItems;
        QStringList unknownMapScraps;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            QVERIFY2(diagnostic.kind != ProjectIndexDiagnosticKind::MixedMapAndScrapReferences,
                     "A missing map item should not count as map or scrap content.");
            if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapItemReference) {
                unknownMapItems.append(QStringLiteral("%1:%2").arg(diagnostic.referencedName).arg(diagnostic.lineNumber));
            } else if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference) {
                unknownMapScraps.append(diagnostic.referencedName);
            }
        }

        unknownMapItems.sort();
        QCOMPARE(unknownMapItems, QStringList({QStringLiteral("PlanMissing2:7"), QStringLiteral("WholeMissing:11")}));
        QCOMPARE(unknownMapScraps, QStringList({QStringLiteral("missing.s")}));
    }
};

int runProjectStructureIndexQTest(int argc, char **argv)
{
    ProjectStructureIndexQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureIndexQTest.moc"
