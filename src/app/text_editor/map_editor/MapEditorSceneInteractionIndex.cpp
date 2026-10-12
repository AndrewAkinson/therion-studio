#include "MapEditorSceneInteractionIndex.h"

#include "MapEditorSceneInternals.h"

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace TherionStudio
{
const MapEditorSceneInteractionIndex::ItemsByLine &MapEditorSceneInteractionIndex::itemsByLine(
    QGraphicsScene *scene, quint64 generation)
{
    if (valid_ && indexedScene_ == scene && indexedGeneration_ == generation) {
        return itemsByLine_;
    }

    itemsByLine_.clear();
    if (scene != nullptr) {
        for (QGraphicsItem *item : scene->items()) {
            if (item == nullptr) {
                continue;
            }
            const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
            if (lineNumber > 0) {
                itemsByLine_[lineNumber].append(item);
            }
        }
    }
    indexedScene_ = scene;
    indexedGeneration_ = generation;
    valid_ = true;
    return itemsByLine_;
}

void MapEditorSceneInteractionIndex::invalidate()
{
    itemsByLine_.clear();
    indexedScene_ = nullptr;
    indexedGeneration_ = 0;
    valid_ = false;
    presentedSelectionLines_.clear();
    presentationScene_ = nullptr;
    presentationGeneration_ = 0;
    presentationValid_ = false;
}

bool MapEditorSceneInteractionIndex::hasSelectionPresentation(QGraphicsScene *scene, quint64 generation) const
{
    return presentationValid_ && scene != nullptr && presentationScene_ == scene
        && presentationGeneration_ == generation;
}

const QSet<int> &MapEditorSceneInteractionIndex::presentedSelectionLines() const
{
    return presentedSelectionLines_;
}

void MapEditorSceneInteractionIndex::recordSelectionPresentation(QGraphicsScene *scene, quint64 generation,
                                                                 const QSet<int> &lines)
{
    presentedSelectionLines_ = lines;
    presentationScene_ = scene;
    presentationGeneration_ = generation;
    presentationValid_ = scene != nullptr;
}
}
