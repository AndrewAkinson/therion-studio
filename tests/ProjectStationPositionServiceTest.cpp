#include "../src/app/ProjectStationPositionService.h"
#include "../src/core/ProjectStructureIndex.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// The service runs Therion with only its config file name; this test binary stands in for Therion then.
const char *const kConfigArgument = "station-positions.thconfig";

bool writeStationExport(const QString &path)
{
    QFile sql(path);
    if (!sql.open(QIODevice::WriteOnly)) {
        return false;
    }
    return sql.write("create table SURVEY (ID integer, PARENT_ID integer, NAME varchar(4), FULL_NAME varchar(9), TITLE varchar(4));\n"
              "create table CENTRELINE (ID integer, SURVEY_ID integer, TITLE varchar(4), TOPO_DATE date, EXPLO_DATE date, "
              "LENGTH real, SURFACE_LENGTH real, DUPLICATE_LENGTH real);\n"
              "create table PERSON (ID integer, NAME varchar(8), SURNAME varchar(10));\n"
              "create table EXPLO (PERSON_ID integer, CENTRELINE_ID integer);\n"
              "create table TOPO (PERSON_ID integer, CENTRELINE_ID integer);\n"
              "create table STATION (ID integer, NAME varchar(5), SURVEY_ID integer, X real, Y real, Z real);\n"
              "create table STATION_FLAG (STATION_ID integer, FLAG char(3));\n"
              "create table SHOT (ID integer, FROM_ID integer, TO_ID integer, CENTRELINE_ID integer, LENGTH real, "
              "BEARING real, GRADIENT real, ADJ_LENGTH real, ADJ_BEARING real, ADJ_GRADIENT real, ERR_LENGTH real, "
              "ERR_BEARING real, ERR_GRADIENT real);\n"
              "create table SHOT_FLAG (SHOT_ID integer, FLAG char(3));\n"
              "insert into SURVEY values (1, 0, 'a', 'a.cave', NULL);\n"
              "insert into STATION values (1, '1', 1, 10.0, 20.0, 30.0);\n"
              "insert into STATION values (2, '-', 1, 0.0, 0.0, 0.0);\n"
              "insert into STATION values (3, '2', 1, 10.0, 25.0, 30.0);\n"
              "insert into SHOT values (1, 1, 3, 1, 5.0, 0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT values (2, 1, 2, 1, 2.0, 90.0, 0.0, 2.0, 90.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT_FLAG values (2, 'spl');\n"
              "insert into SHOT values (3, 3, 1, 1, 5.0, 180.0, 0.0, 5.0, 180.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT_FLAG values (3, 'dpl');\n") > 0;
}

int runFakeTherion()
{
    QFile config(QString::fromLatin1(kConfigArgument));
    if (!config.open(QIODevice::ReadOnly) || !config.readAll().contains("export database -fmt sql")) {
        return 1;
    }
    return writeStationExport(QStringLiteral("station-positions.sql")) ? 0 : 1;
}

QString writeProject(const QTemporaryDir &dir)
{
    QFile source(QDir(dir.path()).filePath(QStringLiteral("cave.th")));
    QFile config(QDir(dir.path()).filePath(QStringLiteral("thconfig")));
    if (!source.open(QIODevice::WriteOnly) || !config.open(QIODevice::WriteOnly)) {
        return {};
    }
    config.write("source cave.th\nexport map -o cave.pdf\n");
    return config.fileName();
}
}

class ProjectStationPositionServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void exportsAndLoadsPositionsOutsideTheProject()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        QVERIFY(!configPath.isEmpty());

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        QSignalSpy failed(&service, &ProjectStationPositionService::positionsFailed);
        const ProjectStationPositionRequest request{QCoreApplication::applicationFilePath(),
                                                    configPath,
                                                    {QDir(project.path()).filePath(QStringLiteral("cave.th"))}};
        service.request(request);
        QVERIFY(service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(!service.isRunning());
        QCOMPARE(service.positions().size(), 2);
        const Th2StationPosition station =
            service.positions().value(ProjectStructureIndex::stationKey(QStringLiteral("1"), QStringLiteral("a.cave")));
        QCOMPARE(station.y, 20.0);
        // Splays and duplicates are left out of the measured legs.
        QCOMPARE(service.legs().size(), 1);
        QCOMPARE(service.legs().first().toStationKey, ProjectStructureIndex::stationKey(QStringLiteral("2"), QStringLiteral("a.cave")));
        QCOMPARE(service.legs().first().vector.y, 5.0);
        // Nothing is written into the project.
        QCOMPARE(QDir(project.path()).entryList(QDir::Files).size(), 2);

        // Up to date: a second request does not run Therion again.
        service.request(request);
        QVERIFY(!service.isRunning());

        // A newer drawing does not make them stale; a newer .th does.
        const QDateTime later = QDateTime::currentDateTime().addSecs(60);
        QFile drawing(QDir(project.path()).filePath(QStringLiteral("cave.th2")));
        QVERIFY(drawing.open(QIODevice::WriteOnly));
        QVERIFY(drawing.setFileTime(later, QFileDevice::FileModificationTime));
        ProjectStationPositionRequest withDrawing = request;
        withDrawing.sourceFiles.append(drawing.fileName());
        service.request(withDrawing);
        QVERIFY(!service.isRunning());
        QFile source(request.sourceFiles.first());
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(later, QFileDevice::FileModificationTime));
        service.request(withDrawing);
        QVERIFY(service.isRunning());
        QVERIFY(ready.wait(10000));

        service.clear();
        QVERIFY(service.positions().isEmpty());
        QVERIFY(service.legs().isEmpty());
    }

    void usesTheCompileExportWithoutRunningTherion()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        const QString exportPath = QDir(project.path()).filePath(QStringLiteral("cave.sql"));
        QVERIFY(writeStationExport(exportPath));

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        service.loadCompileExport(exportPath);
        QVERIFY(!service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(service.positions().size(), 2);
        QCOMPARE(service.legs().size(), 1);

        // A newer .th does not start a run: compile data is kept until the next compile.
        QFile source(QDir(project.path()).filePath(QStringLiteral("cave.th")));
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        service.request(ProjectStationPositionRequest{QCoreApplication::applicationFilePath(), configPath, {source.fileName()}});
        QVERIFY(!service.isRunning());
        QCOMPARE(service.positions().size(), 2);
    }

    void compileExportReplacesARunInProgress()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        const QString exportPath = QDir(project.path()).filePath(QStringLiteral("cave.sql"));
        QVERIFY(writeStationExport(exportPath));

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        service.request(ProjectStationPositionRequest{QCoreApplication::applicationFilePath(), configPath, {}});
        QVERIFY(service.isRunning());
        service.loadCompileExport(exportPath);
        QVERIFY(!service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(ready.count(), 1);
        QCOMPARE(service.positions().size(), 2);
    }

    void reportsAFailedRunOnce()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        const QString configPath = writeProject(project);
        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy failed(&service, &ProjectStationPositionService::positionsFailed);
        const ProjectStationPositionRequest request{QDir(project.path()).filePath(QStringLiteral("no-therion")),
                                                    configPath,
                                                    {}};
        service.request(request);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);
        QVERIFY(!service.isRunning());
        // Not retried until a source, the config or the executable changes.
        service.request(request);
        QVERIFY(!service.isRunning());
        QCOMPARE(failed.count(), 1);
    }
};

int main(int argc, char **argv)
{
    if (argc == 2 && qstrcmp(argv[1], kConfigArgument) == 0) {
        QCoreApplication app(argc, argv);
        return runFakeTherion();
    }
    QCoreApplication app(argc, argv);
    ProjectStationPositionServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStationPositionServiceTest.moc"
