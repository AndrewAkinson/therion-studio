#include "MapEditorJoinPairs.h"

#include "MapEditorJoinOutline.h"
#include "MapEditorJoinViewLink.h"
#include "MapEditorTab.h"

#include <QWidget>

namespace TherionStudio
{
namespace
{
// Width of cave shown across the first view when a join opens.
constexpr qreal kJoinViewWidthMetres = 30.0;

MapEditorJoinViewLink *joinLinkBetween(MapEditorTab *first, MapEditorTab *second)
{
    for (MapEditorTab *owner : {first, second}) {
        for (MapEditorJoinViewLink *link : owner->findChildren<MapEditorJoinViewLink *>(Qt::FindDirectChildrenOnly)) {
            if (link->connects(first, second)) {
                return link;
            }
        }
    }
    return nullptr;
}
}

bool openMapEditorJoinPair(MapEditorTab *first,
                           int firstJoinLine,
                           MapEditorTab *second,
                           int secondJoinLine,
                           bool separateSecondMap,
                           const MapEditorJoinPlacement &placement)
{
    if (first == nullptr || second == nullptr || first == second) {
        return false;
    }
    const std::optional<QPointF> firstJoin = first->stationPointSourcePosition(firstJoinLine);
    const std::optional<QPointF> secondJoin = second->stationPointSourcePosition(secondJoinLine);
    Th2CentrelineFit firstFit;
    Th2CentrelineFit secondFit;
    if (placement.positions != nullptr) {
        firstFit = first->stationPositionFitForSourceLine(
            firstJoinLine, placement.firstStationKeysByLine, *placement.positions, placement.projection);
        secondFit = second->stationPositionFitForSourceLine(
            secondJoinLine, placement.secondStationKeysByLine, *placement.positions, placement.projection);
    }
    const bool sharedFrame = firstFit.isValid() && secondFit.isValid();
    std::optional<Th2SimilarityTransform> placed;
    if (sharedFrame) {
        placed = sharedFramePartnerToThisTransform(firstFit, secondFit);
    } else if (firstJoin.has_value() && secondJoin.has_value()) {
        firstFit = first->centrelineFitForSourceLine(firstJoinLine);
        secondFit = second->centrelineFitForSourceLine(secondJoinLine);
        if (firstFit.isValid() && secondFit.isValid()) {
            placed = joinPartnerToThisTransform(firstFit, *firstJoin, secondFit, *secondJoin);
        }
    }
    if (!placed.has_value()) {
        return false;
    }
    const Th2SimilarityTransform secondToFirst = *placed;
    // With no join station, look at the middle of the two scraps' station points.
    std::optional<QPointF> viewCentre = firstJoin;
    if (!viewCentre.has_value()) {
        const std::optional<QPointF> firstMiddle = first->scrapStationCentroidForSourceLine(firstJoinLine);
        const std::optional<QPointF> secondMiddle = second->scrapStationCentroidForSourceLine(secondJoinLine);
        if (!firstMiddle.has_value() || !secondMiddle.has_value()) {
            return false;
        }
        viewCentre = (*firstMiddle + secondToFirst.map(*secondMiddle)) / 2.0;
    }

    closeMapEditorJoinPair(first, second);
    // Separate before aligning; the link follows any viewport resize while the window is laid out.
    if (separateSecondMap && !second->isMapPaneDetached()) {
        second->toggleMapPaneWindow();
    }
    MapEditorJoinOutline::attach(first, second, secondToFirst);
    MapEditorJoinOutline::attach(second, first, secondToFirst.inverted());

    auto *link = new MapEditorJoinViewLink(first, second, secondToFirst, first);
    QObject::connect(second, &QObject::destroyed, link, &QObject::deleteLater);
    const qreal firstUnitsAcross = kJoinViewWidthMetres / firstFit.drawingToMetres.scale();
    const int viewWidthPixels = qMax(1, first->mapViewportPixelWidth());
    link->showFirstAt(*viewCentre, viewWidthPixels / firstUnitsAcross);
    return true;
}

QVector<MapEditorJoinPair> mapEditorJoinPairsFor(MapEditorTab *tab)
{
    QVector<MapEditorJoinPair> pairs;
    if (tab == nullptr) {
        return pairs;
    }
    // Every join shows an outline in both drawings, so the outlines name the partners.
    for (MapEditorJoinOutline *outline : tab->findChildren<MapEditorJoinOutline *>(Qt::FindDirectChildrenOnly)) {
        MapEditorTab *partner = outline->partner();
        if (partner != nullptr) {
            pairs.append(MapEditorJoinPair{partner, joinLinkBetween(tab, partner), outline});
        }
    }
    return pairs;
}

bool mapEditorHasJoinLink(MapEditorTab *tab)
{
    for (const MapEditorJoinPair &pair : mapEditorJoinPairsFor(tab)) {
        if (pair.link != nullptr) {
            return true;
        }
    }
    return false;
}

void closeMapEditorJoinPair(MapEditorTab *first, MapEditorTab *second)
{
    if (first == nullptr || second == nullptr) {
        return;
    }
    delete joinLinkBetween(first, second);
    delete MapEditorJoinOutline::find(first, second);
    delete MapEditorJoinOutline::find(second, first);
}
}
