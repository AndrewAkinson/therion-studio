#pragma once

#include "../core/Th2CentrelineAlignment.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QTemporaryDir;

namespace TherionStudio
{
class TherionRunnerService;

struct ProjectStationPositionRequest
{
    QString executableInput;
    QString configPath;
    // Project source files; the export is redone when any of them is newer than it.
    QStringList sourceFiles;
};

// Station positions computed by Therion for the whole project, used to place joined drawings in one frame.
// Runs Therion in the background on its own runner (the Compiler panel stays free) with a temporary config that
// only exports the database, in a temporary folder outside the project. One run at a time: a request while running
// is queued once.
class ProjectStationPositionService final : public QObject
{
    Q_OBJECT

public:
    // `temporaryBasePath` holds the temporary folder (chosen at the composition boundary).
    explicit ProjectStationPositionService(const QString &temporaryBasePath, QObject *parent = nullptr);
    ~ProjectStationPositionService() override;

    // Starts an export when there are no positions for this config yet or a source file is newer than them.
    void request(const ProjectStationPositionRequest &request);
    // Forgets positions and stops a run, for example when the project closes.
    void clear();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] const QHash<QString, Th2StationPosition> &positions() const;

signals:
    void runningChanged(bool running);
    void positionsReady();
    void positionsFailed(const QString &message);

private:
    struct LoadResult
    {
        QHash<QString, Th2StationPosition> positions;
        QString errorMessage;
        quint64 generation = 0;
    };

    [[nodiscard]] bool isCurrent(const ProjectStationPositionRequest &request) const;
    void start(const ProjectStationPositionRequest &request);
    void handleRunFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleLoadFinished();
    void finish(const QString &errorMessage);
    [[nodiscard]] static LoadResult loadPositions(const QString &sqlPath);

    QString temporaryBasePath_;
    std::unique_ptr<QTemporaryDir> temporaryDir_;
    TherionRunnerService *runner_ = nullptr;
    QFutureWatcher<LoadResult> *loadWatcher_ = nullptr;
    std::optional<ProjectStationPositionRequest> queued_;
    // Config and executable of the last attempt, and when it started; sources newer than that make it stale.
    QString attemptKey_;
    QDateTime attemptStartedAt_;
    QHash<QString, Th2StationPosition> positions_;
    bool running_ = false;
    quint64 generation_ = 0;
};
}
