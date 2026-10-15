#include "MainWindow.h"

#include "ProjectJoinsPanel.h"
#include "ProjectStationPositionService.h"
#include "text_editor/map_editor/MapEditorJoinPairs.h"
#include "text_editor/map_editor/MapEditorJoinViewLink.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QDir>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>

namespace
{
QHash<int, QString> stationKeysByLine(const TherionStudio::ProjectIndexSnapshot &snapshot, const QString &scrapObjectId)
{
    QHash<int, QString> keys;
    for (const TherionStudio::ProjectScrapStationPoint &point : snapshot.stationPointsByScrapObjectId.value(scrapObjectId)) {
        keys.insert(point.lineNumber, point.stationKey);
    }
    return keys;
}
}

void MainWindow::setupJoinStationPositions()
{
    // Temporary files go in the system temporary folder, never in the project.
    stationPositionService_ = new TherionStudio::ProjectStationPositionService(QDir::tempPath(), this);
    connect(stationPositionService_, &TherionStudio::ProjectStationPositionService::runningChanged, this, [this](bool running) {
        if (running) {
            statusBar()->showMessage(tr("Computing station positions with Therion for joined drawings..."));
        } else {
            statusBar()->clearMessage();
        }
    });
    connect(stationPositionService_,
            &TherionStudio::ProjectStationPositionService::positionsFailed,
            this,
            [this](const QString &message) { statusBar()->showMessage(message, 8000); });
}

void MainWindow::requestJoinStationPositions()
{
    const TherionStudio::ProjectIndexSnapshot &snapshot = lastStructureSidebarProjectIndex_;
    if (stationPositionService_ == nullptr || snapshot.rootConfigPath.isEmpty()
        || (snapshot.equateLinks.isEmpty() && snapshot.joinCommands.isEmpty())) {
        return;
    }
    QSet<QString> sourceFiles = {snapshot.rootConfigPath};
    for (const TherionStudio::ProjectStructureEntry &entry : snapshot.entries) {
        if (!entry.sourceFile.isEmpty()) {
            sourceFiles.insert(entry.sourceFile);
        }
    }
    const QString executable = therionExecutableInput();
    stationPositionService_->request(TherionStudio::ProjectStationPositionRequest{
        executable.isEmpty() ? QStringLiteral("therion") : executable,
        snapshot.rootConfigPath,
        QStringList(sourceFiles.cbegin(), sourceFiles.cend())});
}

TherionStudio::MapEditorJoinPlacement MainWindow::joinPlacement(const QString &firstScrapObjectId,
                                                                const QString &secondScrapObjectId,
                                                                const QString &projection) const
{
    TherionStudio::MapEditorJoinPlacement placement;
    if (stationPositionService_ == nullptr || stationPositionService_->isRunning()
        || stationPositionService_->positions().isEmpty()) {
        return placement;
    }
    placement.positions = &stationPositionService_->positions();
    placement.firstStationKeysByLine = stationKeysByLine(lastStructureSidebarProjectIndex_, firstScrapObjectId);
    placement.secondStationKeysByLine = stationKeysByLine(lastStructureSidebarProjectIndex_, secondScrapObjectId);
    placement.projection = projection;
    return placement;
}

void MainWindow::openProjectJoin(const TherionStudio::ProjectJoinPair &pair, bool separateWindow)
{
    if (!pair.isAvailable()) {
        return;
    }
    requestJoinStationPositions();
    // Each drawing opens centred on its join-station point.
    openProjectJoinDrawings(pair.first.drawing->sourceFile,
                            pair.first.drawing->lineNumber,
                            pair.second.drawing->sourceFile,
                            pair.second.drawing->lineNumber,
                            separateWindow,
                            joinPlacement(pair.first.drawing->scrapObjectId,
                                          pair.second.drawing->scrapObjectId,
                                          pair.first.drawing->projection),
                            tr("Cannot align the joined drawings: a scrap has no XVI centreline stations or -scale."));
}

void MainWindow::openProjectJoinOnly(const TherionStudio::ProjectJoinGroup &group, bool separateWindow)
{
    if (group.joinOnlyScraps.size() != 2) {
        return;
    }
    requestJoinStationPositions();
    const TherionStudio::ProjectStructureEntry &first = group.joinOnlyScraps.at(0);
    const TherionStudio::ProjectStructureEntry &second = group.joinOnlyScraps.at(1);
    const QString projection = lastStructureSidebarProjectIndex_.scrapProjectionByObjectId.value(first.objectId);
    const QString failure = stationPositionService_ != nullptr && stationPositionService_->isRunning()
        ? tr("No station is shared by these drawings; they will line up once Therion has computed the station "
             "positions. Open the join again then.")
        : tr("No station is shared by these drawings, so they open without alignment.");
    openProjectJoinDrawings(first.sourceFile,
                            first.lineNumber,
                            second.sourceFile,
                            second.lineNumber,
                            separateWindow,
                            joinPlacement(first.objectId, second.objectId, projection),
                            failure);
}

void MainWindow::openProjectJoinDrawings(const QString &firstFile,
                                         int firstLine,
                                         const QString &secondFile,
                                         int secondLine,
                                         bool separateWindow,
                                         const TherionStudio::MapEditorJoinPlacement &placement,
                                         const QString &failureMessage)
{
    auto *firstTab = qobject_cast<TherionStudio::MapEditorTab *>(openProjectSourceLocation(firstFile, firstLine));
    auto *secondTab = qobject_cast<TherionStudio::MapEditorTab *>(openProjectSourceLocation(secondFile, secondLine));
    if (firstTab == nullptr || secondTab == nullptr || firstTab == secondTab) {
        return;
    }

    // Outline each drawing in the other and link their views.
    if (separateWindow) {
        // The main window shows the first drawing; the second drawing's map is in its separate window.
        editorTabs_->setCurrentWidget(firstTab);
    }
    if (!TherionStudio::openMapEditorJoinPair(firstTab, firstLine, secondTab, secondLine, separateWindow, placement)) {
        statusBar()->showMessage(failureMessage, 5000);
    }
    // Keep the Joined indicator and link icons current as the link is toggled, removed, or its drawing closed.
    for (const TherionStudio::MapEditorJoinPair &joinPair : TherionStudio::mapEditorJoinPairsFor(firstTab)) {
        if (joinPair.partner == secondTab && joinPair.link != nullptr) {
            connect(joinPair.link, &TherionStudio::MapEditorJoinViewLink::enabledChanged, this, &MainWindow::refreshDocumentStatusWidgets);
            connect(joinPair.link, &QObject::destroyed, this, &MainWindow::refreshDocumentStatusWidgets, Qt::QueuedConnection);
        }
    }
    refreshDocumentStatusWidgets();
}
