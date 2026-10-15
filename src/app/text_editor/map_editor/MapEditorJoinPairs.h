#pragma once

#include "../../../core/Th2CentrelineAlignment.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class MapEditorJoinOutline;
class MapEditorJoinViewLink;
class MapEditorTab;

// One drawing joined to `tab`: the partner, the shared view link, and the partner's outline shown in `tab`.
struct MapEditorJoinPair
{
    QPointer<MapEditorTab> partner;
    QPointer<MapEditorJoinViewLink> link;
    QPointer<MapEditorJoinOutline> outline;
};

// Station positions computed by Therion and the stations each drawing's station points draw (by source line),
// for placing both drawings in Therion's frame.
struct MapEditorJoinPlacement
{
    const QHash<QString, Th2StationPosition> *positions = nullptr;
    QHash<int, QString> firstStationKeysByLine;
    QHash<int, QString> secondStationKeysByLine;
    QString projection;
};

// Aligns two open drawings, shows each as an outline in the other, links the two views, and zooms both onto the
// join. `firstJoinLine` and `secondJoinLine` are the join-station points, or for a join with no shared station
// any line inside each joined scrap. With computed positions for both scraps (`placement`), both are placed in
// Therion's frame and the view is centred between the scraps when there is no join station; otherwise each
// scrap's centreline fit is linked at the join station. With `separateSecondMap`, the second drawing's map is
// shown in its separate map window. Replaces any existing join between the same two editors. Returns false,
// changing nothing, when either drawing cannot be placed.
bool openMapEditorJoinPair(MapEditorTab *first,
                           int firstJoinLine,
                           MapEditorTab *second,
                           int secondJoinLine,
                           bool separateSecondMap = false,
                           const MapEditorJoinPlacement &placement = {});

[[nodiscard]] QVector<MapEditorJoinPair> mapEditorJoinPairsFor(MapEditorTab *tab);

void closeMapEditorJoinPair(MapEditorTab *first, MapEditorTab *second);

// True while `tab` is part of a join whose views are linked; drives the link icon on its tab and map window.
[[nodiscard]] bool mapEditorHasEnabledJoinLink(MapEditorTab *tab);
}
