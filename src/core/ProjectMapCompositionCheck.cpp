#include "ProjectMapCompositionCheck.h"

#include "TherionProjectionRules.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <functional>

namespace TherionStudio
{
namespace
{
bool holdsScraps(ProjectMapCompositionItemKind kind)
{
    return kind == ProjectMapCompositionItemKind::Scrap;
}

ProjectIndexDiagnostic itemDiagnostic(ProjectIndexDiagnosticKind kind,
                                      const ProjectMapComposition &composition,
                                      const ProjectMapCompositionItem &item)
{
    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = composition.sourceObjectId;
    diagnostic.sourceFile = composition.sourceFile;
    diagnostic.lineNumber = item.lineNumber;
    diagnostic.columnNumber = qMax(1, item.columnNumber);
    diagnostic.columnLength = qMax(0, item.columnLength);
    diagnostic.referencedName = item.name;
    return diagnostic;
}
}

QVector<ProjectIndexDiagnostic> checkMapCompositionProjections(
    const QHash<QString, ProjectMapComposition> &compositionsByMapKey,
    const QHash<QString, QString> &scrapProjectionsByKey)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    QSet<QString> inProgress;
    // Each map's projection identity once worked out, empty when it cannot be known.
    QHash<QString, QString> projectionByMapKey;

    std::function<QString(const QString &)> mapProjection = [&](const QString &mapKey) -> QString {
        if (projectionByMapKey.contains(mapKey)) {
            return projectionByMapKey.value(mapKey);
        }
        const auto compositionIt = compositionsByMapKey.constFind(mapKey);
        if (compositionIt == compositionsByMapKey.constEnd()) {
            return QString();
        }
        const ProjectMapComposition &composition = compositionIt.value();
        const QString explicitProjection = therionProjectionIdentity(composition.projection);

        inProgress.insert(mapKey);
        QString contentProjection;
        bool firstHoldsScraps = false;
        bool contentKnown = false;
        bool projectionReported = false;
        for (const ProjectMapCompositionItem &item : composition.items) {
            if (item.kind == ProjectMapCompositionItemKind::Map && inProgress.contains(item.key)) {
                diagnostics.append(
                    itemDiagnostic(ProjectIndexDiagnosticKind::RecursiveMapReference, composition, item));
                continue;
            }

            QString itemProjection;
            if (item.kind == ProjectMapCompositionItemKind::Map) {
                itemProjection = mapProjection(item.key);
            } else if (item.kind == ProjectMapCompositionItemKind::Scrap) {
                itemProjection = therionProjectionIdentity(
                    scrapProjectionsByKey.value(item.key, QStringLiteral("plan")));
            } else {
                // A survey is drawn in the map's own projection.
                itemProjection = explicitProjection;
            }

            if (!contentKnown) {
                firstHoldsScraps = holdsScraps(item.kind);
            } else if (holdsScraps(item.kind) != firstHoldsScraps) {
                continue;
            }
            if (itemProjection.isEmpty() || projectionReported) {
                contentKnown = true;
                continue;
            }

            const bool differsFromContent = !contentProjection.isEmpty() && itemProjection != contentProjection;
            const bool differsFromMap = !explicitProjection.isEmpty() && itemProjection != explicitProjection;
            if (differsFromContent || differsFromMap) {
                diagnostics.append(itemDiagnostic(holdsScraps(item.kind)
                                                      ? ProjectIndexDiagnosticKind::IncompatibleScrapProjection
                                                      : ProjectIndexDiagnosticKind::IncompatibleMapProjection,
                                                  composition,
                                                  item));
                projectionReported = true;
            } else if (contentProjection.isEmpty()) {
                contentProjection = itemProjection;
            }
            contentKnown = true;
        }
        inProgress.remove(mapKey);

        const QString projection = explicitProjection.isEmpty() ? contentProjection : explicitProjection;
        projectionByMapKey.insert(mapKey, projection);
        return projection;
    };

    QStringList mapKeys = compositionsByMapKey.keys();
    std::sort(mapKeys.begin(), mapKeys.end());
    for (const QString &mapKey : std::as_const(mapKeys)) {
        mapProjection(mapKey);
    }
    return diagnostics;
}
}
