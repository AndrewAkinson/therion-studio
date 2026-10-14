#include "MapEditorJoinViewLink.h"

#include "MapEditorTab.h"

#include <QScopedValueRollback>

namespace TherionStudio
{
MapEditorJoinViewLink::MapEditorJoinViewLink(MapEditorTab *first,
                                             MapEditorTab *second,
                                             const Th2SimilarityTransform &secondToFirst,
                                             QObject *parent)
    : QObject(parent)
    , first_(first)
    , second_(second)
    , secondToFirst_(secondToFirst)
    , firstToSecond_(secondToFirst.inverted())
{
    if (first_ != nullptr) {
        connect(first_, &MapEditorTab::mapViewChanged, this, &MapEditorJoinViewLink::alignSecondToFirst);
    }
    if (second_ != nullptr) {
        connect(second_, &MapEditorTab::mapViewChanged, this, &MapEditorJoinViewLink::alignFirstToSecond);
    }
}

void MapEditorJoinViewLink::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (enabled_) {
        alignSecondToFirst();
    }
}

bool MapEditorJoinViewLink::isEnabled() const
{
    return enabled_;
}

void MapEditorJoinViewLink::showFirstAt(const QPointF &firstCentre, qreal pixelsPerFirstUnit)
{
    if (first_ == nullptr) {
        return;
    }
    {
        const QScopedValueRollback<bool> guard(aligning_, true);
        first_->setMapViewCentreAndScale(firstCentre, pixelsPerFirstUnit);
    }
    alignSecondToFirst();
}

void MapEditorJoinViewLink::alignSecondToFirst()
{
    if (!enabled_ || aligning_ || first_ == nullptr || second_ == nullptr) {
        return;
    }
    const QScopedValueRollback<bool> guard(aligning_, true);
    // One second-drawing unit spans scale() first-drawing units on screen.
    second_->setMapViewCentreAndScale(firstToSecond_.map(first_->mapViewCentreSourcePosition()),
                                      first_->mapViewPixelsPerSourceUnit() * secondToFirst_.scale());
}

void MapEditorJoinViewLink::alignFirstToSecond()
{
    if (!enabled_ || aligning_ || first_ == nullptr || second_ == nullptr) {
        return;
    }
    const QScopedValueRollback<bool> guard(aligning_, true);
    first_->setMapViewCentreAndScale(secondToFirst_.map(second_->mapViewCentreSourcePosition()),
                                     second_->mapViewPixelsPerSourceUnit() / secondToFirst_.scale());
}
}
