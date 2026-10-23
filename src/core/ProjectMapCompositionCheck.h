#pragma once

#include "ProjectStructureIndex.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace TherionStudio
{
enum class ProjectMapCompositionItemKind
{
    Map,
    Scrap,
    Survey,
};

// A map item that resolved to exactly one map, scrap or survey.
struct ProjectMapCompositionItem
{
    ProjectMapCompositionItemKind kind = ProjectMapCompositionItemKind::Scrap;
    QString key;
    QString name;
    int lineNumber = 0;
    int columnNumber = 1;
    int columnLength = 0;
};

struct ProjectMapComposition
{
    QString sourceObjectId;
    QString sourceFile;
    // The map's `-projection` as written, empty when it has none.
    QString projection;
    QVector<ProjectMapCompositionItem> items;
};

// The checks Therion makes while it works out each map's projection (`thdb2d::process_map_references`):
// a map that contains itself, and items whose projection differs from the map's first item or its `-projection`.
// Items Therion would reject for another reason (unresolved, or mixed scraps and maps) are left to those checks.
[[nodiscard]] QVector<ProjectIndexDiagnostic> checkMapCompositionProjections(
    const QHash<QString, ProjectMapComposition> &compositionsByMapKey,
    const QHash<QString, QString> &scrapProjectionsByKey);
}
