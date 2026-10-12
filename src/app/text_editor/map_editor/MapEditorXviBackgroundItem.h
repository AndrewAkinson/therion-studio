#pragma once

#include <QColor>
#include <QGraphicsPixmapItem>
#include <QLineF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorXviLineTile
{
    QRectF bounds;
    QVector<int> lineIndexes;
};

struct MapEditorXviSketchPathData
{
    QVector<QLineF> lines;
    QVector<MapEditorXviLineTile> tiles;
    QColor color;
    Qt::PenStyle style = Qt::SolidLine;
};

struct MapEditorXviPassagePolygonData
{
    QPolygonF polygon;
    QRectF bounds;
};

struct MapEditorXviPassageTile
{
    QRectF bounds;
    QVector<int> polygonIndexes;
};

struct MapEditorXviStationData
{
    QString name;
    QPointF position;
};

struct MapEditorXviTraverseStations
{
    QString from;
    QString to;
};

struct MapEditorXviTraverseColor
{
    QColor from;
    QColor to;
    bool hasAltitude = false;

    bool operator==(const MapEditorXviTraverseColor &) const = default;
};

struct MapEditorXviLayerGeometryData
{
    QVector<QLineF> gridLines;
    QVector<MapEditorXviLineTile> gridTiles;
    QVector<QLineF> traverseShotLines;
    QVector<MapEditorXviTraverseStations> traverseShotStations;
    QVector<MapEditorXviLineTile> traverseShotTiles;
    QVector<QLineF> splayShotLines;
    QVector<MapEditorXviLineTile> splayShotTiles;
    QVector<MapEditorXviPassagePolygonData> passagePolygons;
    QVector<MapEditorXviPassageTile> passageTiles;
    QVector<MapEditorXviSketchPathData> sketchPaths;
    QVector<MapEditorXviStationData> stations;
    QRectF contentBounds;

    bool hasContent() const
    {
        bool hasSketch = false;
        for (const MapEditorXviSketchPathData &sketch : sketchPaths) {
            if (!sketch.lines.isEmpty()) {
                hasSketch = true;
                break;
            }
        }
        return !contentBounds.isEmpty()
            && (!gridLines.isEmpty()
                || !traverseShotLines.isEmpty()
                || !splayShotLines.isEmpty()
                || !passagePolygons.isEmpty()
                || hasSketch
                || !stations.isEmpty());
    }
};

class MapEditorXviBackgroundItem final : public QGraphicsPixmapItem
{
public:
    explicit MapEditorXviBackgroundItem(QGraphicsItem *parent = nullptr);

    void setGeometryData(const MapEditorXviLayerGeometryData &geometry);
    const MapEditorXviLayerGeometryData &geometryData() const;
    void setCenterlineAppearance(const QColor &color, qreal width);
    void setCenterlineAltitudeColors(const QVector<MapEditorXviTraverseColor> &colors, bool enabled);
    const QVector<MapEditorXviTraverseColor> &centerlineAltitudeColors() const { return centerlineAltitudeColors_; }
    QColor centerlineColor() const { return centerlineColor_; }
    qreal centerlineWidth() const { return centerlineWidth_; }
    void refreshPaintBounds();

    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    MapEditorXviLayerGeometryData geometry_;
    QRectF paintBounds_;
    QColor centerlineColor_ = QColor(18, 44, 88, 130);
    qreal centerlineWidth_ = 1.8;
    QVector<MapEditorXviTraverseColor> centerlineAltitudeColors_;
    bool centerlineAltitudeColorsEnabled_ = false;
};
}
