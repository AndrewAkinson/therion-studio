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
                           bool separateSecondMap)
{
    if (first == nullptr || second == nullptr || first == second) {
        return false;
    }
    const Th2CentrelineFit firstFit = first->centrelineFitForSourceLine(firstJoinLine);
    const Th2CentrelineFit secondFit = second->centrelineFitForSourceLine(secondJoinLine);
    const std::optional<QPointF> firstJoin = first->stationPointSourcePosition(firstJoinLine);
    const std::optional<QPointF> secondJoin = second->stationPointSourcePosition(secondJoinLine);
    if (!firstFit.isValid() || !secondFit.isValid() || !firstJoin.has_value() || !secondJoin.has_value()) {
        return false;
    }

    closeMapEditorJoinPair(first, second);
    // Separate before aligning; the link follows any viewport resize while the window is laid out.
    if (separateSecondMap && !second->isMapPaneDetached()) {
        second->toggleMapPaneWindow();
    }
    const Th2SimilarityTransform secondToFirst = joinPartnerToThisTransform(firstFit, *firstJoin, secondFit, *secondJoin);
    MapEditorJoinOutline::attach(first, second, secondToFirst);
    MapEditorJoinOutline::attach(second, first, secondToFirst.inverted());

    auto *link = new MapEditorJoinViewLink(first, second, secondToFirst, first);
    QObject::connect(second, &QObject::destroyed, link, &QObject::deleteLater);
    const qreal firstUnitsAcross = kJoinViewWidthMetres / firstFit.drawingToMetres.scale();
    const int viewWidthPixels = qMax(1, first->mapViewportPixelWidth());
    link->showFirstAt(*firstJoin, viewWidthPixels / firstUnitsAcross);
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

bool mapEditorHasEnabledJoinLink(MapEditorTab *tab)
{
    for (const MapEditorJoinPair &pair : mapEditorJoinPairsFor(tab)) {
        if (pair.link != nullptr && pair.link->isEnabled()) {
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
