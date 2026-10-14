#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;
struct TherionXviDocument;

// 2D similarity without reflection: p' = [a -b; b a] p + (tx, ty).
struct Th2SimilarityTransform
{
    qreal a = 1.0;
    qreal b = 0.0;
    qreal tx = 0.0;
    qreal ty = 0.0;

    [[nodiscard]] static Th2SimilarityTransform fromScale(qreal scale);
    [[nodiscard]] static Th2SimilarityTransform fromTranslation(const QPointF &offset);
    [[nodiscard]] QPointF map(const QPointF &point) const;
    [[nodiscard]] Th2SimilarityTransform inverted() const;
    // Applies this transform, then `next`.
    [[nodiscard]] Th2SimilarityTransform then(const Th2SimilarityTransform &next) const;
    [[nodiscard]] qreal scale() const;
};

struct Th2ScrapStationPoint
{
    QString stationName;
    QPointF position;
    int lineNumber = 0;
};

struct Th2StationFitPair
{
    QString stationName;
    QPointF drawingPosition;
    QPointF centrelineMetres;
};

enum class Th2CentrelineFitSource
{
    None,
    XviStations,
    ScrapScale,
};

// Similarity transform from scrap drawing coordinates to survey-local metres, found the way
// Therion places a scrap: station points matched to centreline positions.
struct Th2CentrelineFit
{
    Th2CentrelineFitSource source = Th2CentrelineFitSource::None;
    Th2SimilarityTransform drawingToMetres;
    int stationCount = 0;
    qreal rmsResidualMetres = 0.0;

    [[nodiscard]] bool isValid() const { return source != Th2CentrelineFitSource::None; }
};

[[nodiscard]] std::optional<qreal> therionLengthUnitMetres(const QString &unit);

// Station points of the scrap that starts at `scrapStartLine` (no scrap station-name transform applied).
[[nodiscard]] QVector<Th2ScrapStationPoint> th2ScrapStationPoints(const QVector<TherionSourceLogicalCommand> &commands,
                                                                  int scrapStartLine);

// Metres per drawing unit from the XVI grid (`XVIgrid` step length per `XVIgrids` real length).
[[nodiscard]] std::optional<qreal> xviMetresPerDrawingUnit(const TherionXviDocument &xvi);

// Pairs scrap station points with XVI centreline stations of the same name, in XVI metres.
[[nodiscard]] QVector<Th2StationFitPair> xviStationFitPairs(const QVector<Th2ScrapStationPoint> &points,
                                                            const TherionXviDocument &xvi);

// The scrap `-scale` option as a drawing-to-metres similarity (rotation and scale; translation is arbitrary).
[[nodiscard]] std::optional<Th2SimilarityTransform> th2ScrapScaleTransform(const QVector<TherionSourceLogicalCommand> &commands,
                                                               int scrapStartLine);

// Least-squares similarity fit of drawing positions onto centreline metres; needs two or more pairs.
[[nodiscard]] std::optional<Th2SimilarityTransform> similarityFit(const QVector<Th2StationFitPair> &pairs,
                                                      qreal *rmsResidual = nullptr);

// Two or more XVI pairs: fitted similarity. One pair: XVI grid scale, no rotation. Otherwise scrap `-scale`.
[[nodiscard]] Th2CentrelineFit fitTh2ScrapToCentreline(const QVector<Th2StationFitPair> &xviPairs,
                                                       std::optional<qreal> xviMetresPerUnit,
                                                       std::optional<Th2SimilarityTransform> scrapScaleTransform);

// Start line of the scrap that contains `lineNumber`, or 0 when the line is outside any scrap.
[[nodiscard]] int th2ScrapStartLineContaining(const QVector<TherionSourceLogicalCommand> &commands, int lineNumber);

// Fits the scrap containing `lineNumber`, using whichever XVI matches most of its station points.
[[nodiscard]] Th2CentrelineFit fitTh2ScrapContainingLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                         int lineNumber,
                                                         const QVector<TherionXviDocument> &xviDocuments);

// Maps partner drawing coordinates into this drawing so the two join station points coincide.
[[nodiscard]] Th2SimilarityTransform joinPartnerToThisTransform(const Th2CentrelineFit &thisFit,
                                                    const QPointF &thisJoinPoint,
                                                    const Th2CentrelineFit &partnerFit,
                                                    const QPointF &partnerJoinPoint);
}
