#pragma once

#include <QPointer>
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

// Aligns two open drawings at their join-station points (source line numbers) through each scrap's centreline
// fit, shows each as an outline in the other, links the two views, and zooms both onto the join. With
// `separateSecondMap`, the second drawing's map is shown in its separate map window. Replaces any existing join
// between the same two editors. Returns false, changing nothing, when either drawing cannot be placed.
bool openMapEditorJoinPair(MapEditorTab *first,
                           int firstJoinLine,
                           MapEditorTab *second,
                           int secondJoinLine,
                           bool separateSecondMap = false);

[[nodiscard]] QVector<MapEditorJoinPair> mapEditorJoinPairsFor(MapEditorTab *tab);

void closeMapEditorJoinPair(MapEditorTab *first, MapEditorTab *second);
}
