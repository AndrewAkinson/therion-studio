#include "MapEditorTab.h"

#include "MapEditorBackgroundAssetLoader.h"
#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionBackgroundMetadata.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>

#include <cmath>

namespace TherionStudio
{
namespace
{
bool sourceProjectionUsable(const QRectF &sourceBounds, const QRectF &previewBounds)
{
    return previewBounds.isValid() && sourceBounds.isValid() && sourceBounds.width() >= 0.001
        && sourceBounds.height() >= 0.001;
}
}

QPointF MapEditorTab::mapViewCentreSourcePosition() const
{
    if (mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return QPointF();
    }
    const QPointF sceneCentre = mapView_->mapToScene(mapView_->viewport()->rect().center());
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    const QRectF previewBounds = mapPreviewBounds();
    // Inverse of scenePointFromSourcePosition, using the same bounds.
    return sourceProjectionUsable(sourceBounds, previewBounds)
        ? mapGeometryPreviewToSource(sceneCentre, sourceBounds, previewBounds)
        : sceneCentre;
}

qreal MapEditorTab::mapViewPixelsPerSourceUnit() const
{
    if (mapView_ == nullptr) {
        return 0.0;
    }
    const QPointF origin = mapViewCentreSourcePosition();
    const qreal sceneUnitsPerSourceUnit =
        QLineF(scenePointFromSourcePosition(origin), scenePointFromSourcePosition(origin + QPointF(1.0, 0.0))).length();
    const QTransform viewTransform = mapView_->transform();
    return std::hypot(viewTransform.m11(), viewTransform.m21()) * sceneUnitsPerSourceUnit;
}

void MapEditorTab::setMapViewCentreAndScale(const QPointF &sourceCentre, qreal pixelsPerSourceUnit)
{
    if (mapView_ == nullptr || pixelsPerSourceUnit <= 0.0) {
        return;
    }
    const qreal currentPixelsPerSourceUnit = mapViewPixelsPerSourceUnit();
    if (currentPixelsPerSourceUnit > 0.0 && !qFuzzyCompare(currentPixelsPerSourceUnit, pixelsPerSourceUnit)) {
        // Shared zoom path: keeps zoom limits, auto-fit state, and zoom status in step.
        adjustMapZoom(pixelsPerSourceUnit / currentPixelsPerSourceUnit);
    }
    const QPointF sceneCentre = scenePointFromSourcePosition(sourceCentre);
    ensureMapViewCanCentreOn(sceneCentre);
    mapView_->centerOn(sceneCentre);
}

void MapEditorTab::ensureMapViewCanCentreOn(const QPointF &sceneCentre)
{
    if (mapView_ == nullptr || mapScene_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    // A linked view must be able to centre any point, including one at the drawing's edge. Widen the
    // view's own scroll area rather than the scene rect, which scene refreshes own.
    const QRectF visibleScene = mapView_->mapToScene(mapView_->viewport()->rect()).boundingRect();
    QRectF needed(QPointF(), visibleScene.size());
    needed.moveCenter(sceneCentre);
    const QRectF current = mapViewScrollAreaExtended_ ? mapView_->sceneRect() : mapScene_->sceneRect();
    if (current.contains(needed)) {
        return;
    }
    if (!mapViewScrollAreaExtended_) {
        mapViewScrollAreaExtended_ = true;
        connect(mapScene_, &QGraphicsScene::sceneRectChanged, this, [this](const QRectF &sceneRect) {
            if (mapView_ != nullptr) {
                mapView_->setSceneRect(mapView_->sceneRect().united(sceneRect));
            }
        });
    }
    mapView_->setSceneRect(current.united(needed));
}

Th2CentrelineFit MapEditorTab::centrelineFitForSourceLine(int lineNumber)
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (snapshot == nullptr || textEditor_ == nullptr) {
        return Th2CentrelineFit();
    }
    QVector<TherionXviDocument> xviDocuments;
    for (const TherionBackgroundReference &reference :
         parseTherionBackgroundReferences(textEditor_->text(), textEditor_->filePath())) {
        TherionXviDocument xviDocument;
        if (reference.xviReference
            && MapEditorBackgroundAssetLoader::loadXvi(backgroundAssetCache_, reference.absolutePath, &xviDocument)) {
            xviDocuments.append(xviDocument);
        }
    }
    return fitTh2ScrapContainingLine(snapshot->logicalCommands, lineNumber, xviDocuments);
}
}
