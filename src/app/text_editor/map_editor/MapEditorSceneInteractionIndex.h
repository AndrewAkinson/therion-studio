#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QtGlobal>

class QGraphicsItem;
class QGraphicsScene;

namespace TherionStudio
{
class MapEditorSceneInteractionIndex final
{
public:
    using ItemsByLine = QHash<int, QVector<QGraphicsItem *>>;

    const ItemsByLine &itemsByLine(QGraphicsScene *scene, quint64 generation);
    void invalidate();

    // Lines whose items currently carry selection presentation (highlight,
    // focused vertex or visible selection-gated handles). Owned here rather
    // than by the per-call selection controller so delta updates can clear
    // the previously presented lines.
    bool hasSelectionPresentation(QGraphicsScene *scene, quint64 generation) const;
    const QSet<int> &presentedSelectionLines() const;
    void recordSelectionPresentation(QGraphicsScene *scene, quint64 generation, const QSet<int> &lines);

private:
    QGraphicsScene *indexedScene_ = nullptr;
    quint64 indexedGeneration_ = 0;
    bool valid_ = false;
    ItemsByLine itemsByLine_;
    QGraphicsScene *presentationScene_ = nullptr;
    quint64 presentationGeneration_ = 0;
    bool presentationValid_ = false;
    QSet<int> presentedSelectionLines_;
};
}
