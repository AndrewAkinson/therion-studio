#include "Th2CentrelineAlignment.h"

#include "TherionCommandLineModel.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionStationNameRules.h"
#include "TherionXviParser.h"

#include <QHash>
#include <QLineF>
#include <QtMath>

namespace TherionStudio
{
namespace
{
const TherionSourceLogicalCommand *commandStartingAt(const TherionSourceLogicalDocument &document, int startLine)
{
    for (const TherionSourceLogicalCommand &command : document.commands()) {
        if (command.startLineNumber == startLine) {
            return &command;
        }
    }
    return nullptr;
}

std::optional<qreal> numberToken(const QString &token)
{
    bool ok = false;
    const qreal value = token.toDouble(&ok);
    return ok ? std::optional<qreal>(value) : std::nullopt;
}

QString bareStationName(const QString &name)
{
    return name.section(QLatin1Char('@'), 0, 0).trimmed();
}

}

Th2SimilarityTransform Th2SimilarityTransform::fromScale(qreal scale)
{
    return Th2SimilarityTransform{scale, 0.0, 0.0, 0.0};
}

Th2SimilarityTransform Th2SimilarityTransform::fromTranslation(const QPointF &offset)
{
    return Th2SimilarityTransform{1.0, 0.0, offset.x(), offset.y()};
}

QPointF Th2SimilarityTransform::map(const QPointF &point) const
{
    return QPointF(a * point.x() - b * point.y() + tx, b * point.x() + a * point.y() + ty);
}

Th2SimilarityTransform Th2SimilarityTransform::inverted() const
{
    const qreal determinant = a * a + b * b;
    if (determinant <= 0.0) {
        return Th2SimilarityTransform{};
    }
    Th2SimilarityTransform inverse{a / determinant, -b / determinant, 0.0, 0.0};
    const QPointF offset = inverse.map(QPointF(tx, ty));
    inverse.tx = -offset.x();
    inverse.ty = -offset.y();
    return inverse;
}

Th2SimilarityTransform Th2SimilarityTransform::then(const Th2SimilarityTransform &next) const
{
    const QPointF offset = next.map(QPointF(tx, ty));
    return Th2SimilarityTransform{next.a * a - next.b * b, next.a * b + next.b * a, offset.x(), offset.y()};
}

qreal Th2SimilarityTransform::scale() const
{
    return qSqrt(a * a + b * b);
}

std::optional<qreal> therionLengthUnitMetres(const QString &unit)
{
    static const QHash<QString, qreal> metresByUnit = {
        {QStringLiteral("m"), 1.0},          {QStringLiteral("meter"), 1.0},     {QStringLiteral("meters"), 1.0},
        {QStringLiteral("metre"), 1.0},      {QStringLiteral("metres"), 1.0},    {QStringLiteral("cm"), 0.01},
        {QStringLiteral("centimeter"), 0.01}, {QStringLiteral("centimeters"), 0.01},
        {QStringLiteral("centimetre"), 0.01}, {QStringLiteral("centimetres"), 0.01},
        {QStringLiteral("mm"), 0.001},       {QStringLiteral("millimeter"), 0.001},
        {QStringLiteral("millimeters"), 0.001}, {QStringLiteral("millimetre"), 0.001},
        {QStringLiteral("millimetres"), 0.001}, {QStringLiteral("in"), 0.0254},  {QStringLiteral("inch"), 0.0254},
        {QStringLiteral("inches"), 0.0254},  {QStringLiteral("ft"), 0.3048},    {QStringLiteral("feet"), 0.3048},
        {QStringLiteral("yd"), 0.9144},      {QStringLiteral("yard"), 0.9144},  {QStringLiteral("yards"), 0.9144},
    };
    const auto it = metresByUnit.constFind(unit.trimmed().toLower());
    return it == metresByUnit.constEnd() ? std::nullopt : std::optional<qreal>(it.value());
}

QVector<Th2ScrapStationPoint> th2ScrapStationPoints(const TherionSourceLogicalDocument &document, int scrapStartLine)
{
    QVector<Th2ScrapStationPoint> points;
    bool inScrap = false;
    for (const TherionSourceLogicalCommand &command : document.commands()) {
        const QString &commandName = command.metadata.commandName;
        if (commandName == QStringLiteral("scrap")) {
            inScrap = command.startLineNumber == scrapStartLine;
            continue;
        }
        if (!inScrap) {
            continue;
        }
        if (commandName == QStringLiteral("endscrap")) {
            break;
        }
        const std::optional<TherionSourceLogicalArgumentRange> name = therionStationPointNameRange(command);
        if (!name.has_value()) {
            continue;
        }
        const std::optional<qreal> x = numberToken(command.positionalArgumentRanges.at(0).text);
        const std::optional<qreal> y = numberToken(command.positionalArgumentRanges.at(1).text);
        if (x.has_value() && y.has_value()) {
            points.append(Th2ScrapStationPoint{name->text.trimmed(), QPointF(*x, *y), command.startLineNumber});
        }
    }
    return points;
}

std::optional<qreal> xviMetresPerDrawingUnit(const TherionXviDocument &xvi)
{
    if (!xvi.hasGridSpacing) {
        return std::nullopt;
    }
    const qreal stepLength = QLineF(QPointF(), xvi.gridVectorX).length();
    const std::optional<qreal> unitMetres = therionLengthUnitMetres(xvi.gridSpacingUnit);
    if (stepLength <= 0.0 || !unitMetres.has_value()) {
        return std::nullopt;
    }
    return xvi.gridSpacing * *unitMetres / stepLength;
}

QVector<Th2StationFitPair> xviStationFitPairs(const QVector<Th2ScrapStationPoint> &points,
                                              const TherionXviDocument &xvi)
{
    QVector<Th2StationFitPair> pairs;
    const std::optional<qreal> metresPerUnit = xviMetresPerDrawingUnit(xvi);
    if (!metresPerUnit.has_value()) {
        return pairs;
    }
    QHash<QString, QPointF> xviStationsByName;
    for (const TherionXviStation &station : xvi.stationEntries) {
        // XTherion uses the first matching station name.
        if (!xviStationsByName.contains(bareStationName(station.name))) {
            xviStationsByName.insert(bareStationName(station.name), station.position);
        }
    }
    for (const Th2ScrapStationPoint &point : points) {
        const auto it = xviStationsByName.constFind(bareStationName(point.stationName));
        if (it != xviStationsByName.constEnd()) {
            pairs.append(Th2StationFitPair{point.stationName, point.position, it.value() * *metresPerUnit});
        }
    }
    return pairs;
}

std::optional<Th2SimilarityTransform> th2ScrapScaleTransform(const TherionSourceLogicalDocument &document, int scrapStartLine)
{
    const TherionSourceLogicalCommand *scrap = commandStartingAt(document, scrapStartLine);
    if (scrap == nullptr) {
        return std::nullopt;
    }
    const QStringList tokens = commandOptionUnbracketedValueTokens(scrap->parsed.tokens, QStringLiteral("scale"));
    QVector<qreal> numbers;
    for (const QString &token : tokens) {
        if (const std::optional<qreal> number = numberToken(token)) {
            numbers.append(*number);
        }
    }
    const std::optional<qreal> unitMetres =
        tokens.isEmpty() ? std::nullopt : therionLengthUnitMetres(tokens.last());
    if (!unitMetres.has_value()) {
        return std::nullopt;
    }
    // Therion forms: [<length> <units>] real length per drawing unit; [<drawing> <length> <units>];
    // [x1 y1 x2 y2 X1 Y1 X2 Y2 <units>] two drawing points and their real positions.
    if (numbers.size() == 1 && numbers.at(0) > 0.0) {
        return Th2SimilarityTransform::fromScale(numbers.at(0) * *unitMetres);
    }
    if (numbers.size() == 2 && numbers.at(0) > 0.0 && numbers.at(1) > 0.0) {
        return Th2SimilarityTransform::fromScale(numbers.at(1) * *unitMetres / numbers.at(0));
    }
    if (numbers.size() == 8) {
        const QVector<Th2StationFitPair> pairs = {
            {QString(), QPointF(numbers.at(0), numbers.at(1)), QPointF(numbers.at(4), numbers.at(5)) * *unitMetres},
            {QString(), QPointF(numbers.at(2), numbers.at(3)), QPointF(numbers.at(6), numbers.at(7)) * *unitMetres},
        };
        return similarityFit(pairs);
    }
    return std::nullopt;
}

std::optional<Th2SimilarityTransform> similarityFit(const QVector<Th2StationFitPair> &pairs, qreal *rmsResidual)
{
    if (pairs.size() < 2) {
        return std::nullopt;
    }
    QPointF drawingCentroid;
    QPointF metresCentroid;
    for (const Th2StationFitPair &pair : pairs) {
        drawingCentroid += pair.drawingPosition;
        metresCentroid += pair.centrelineMetres;
    }
    drawingCentroid /= pairs.size();
    metresCentroid /= pairs.size();

    // Closed-form 2D least squares for metres = s * R(theta) * drawing + t.
    qreal dot = 0.0;
    qreal cross = 0.0;
    qreal drawingSpread = 0.0;
    for (const Th2StationFitPair &pair : pairs) {
        const QPointF d = pair.drawingPosition - drawingCentroid;
        const QPointF m = pair.centrelineMetres - metresCentroid;
        dot += d.x() * m.x() + d.y() * m.y();
        cross += d.x() * m.y() - d.y() * m.x();
        drawingSpread += d.x() * d.x() + d.y() * d.y();
    }
    if (drawingSpread <= 0.0) {
        return std::nullopt;
    }
    const qreal a = dot / drawingSpread;
    const qreal b = cross / drawingSpread;
    const QPointF t = metresCentroid - QPointF(a * drawingCentroid.x() - b * drawingCentroid.y(),
                                               b * drawingCentroid.x() + a * drawingCentroid.y());
    const Th2SimilarityTransform fit{a, b, t.x(), t.y()};

    if (rmsResidual != nullptr) {
        qreal sum = 0.0;
        for (const Th2StationFitPair &pair : pairs) {
            const QPointF delta = fit.map(pair.drawingPosition) - pair.centrelineMetres;
            sum += delta.x() * delta.x() + delta.y() * delta.y();
        }
        *rmsResidual = qSqrt(sum / pairs.size());
    }
    return fit;
}

Th2CentrelineFit fitTh2ScrapToCentreline(const QVector<Th2StationFitPair> &xviPairs,
                                         std::optional<qreal> xviMetresPerUnit,
                                         std::optional<Th2SimilarityTransform> scrapScaleTransform)
{
    Th2CentrelineFit fit;
    qreal residual = 0.0;
    if (const std::optional<Th2SimilarityTransform> fitted = similarityFit(xviPairs, &residual)) {
        fit.source = Th2CentrelineFitSource::XviStations;
        fit.drawingToMetres = *fitted;
        fit.stationCount = xviPairs.size();
        fit.rmsResidualMetres = residual;
        return fit;
    }
    if (xviPairs.size() == 1 && xviMetresPerUnit.has_value()) {
        const Th2StationFitPair &pair = xviPairs.first();
        const Th2SimilarityTransform scale = Th2SimilarityTransform::fromScale(*xviMetresPerUnit);
        const QPointF offset = pair.centrelineMetres - scale.map(pair.drawingPosition);
        fit.source = Th2CentrelineFitSource::XviStations;
        fit.drawingToMetres = scale.then(Th2SimilarityTransform::fromTranslation(offset));
        fit.stationCount = 1;
        return fit;
    }
    if (scrapScaleTransform.has_value()) {
        fit.source = Th2CentrelineFitSource::ScrapScale;
        fit.drawingToMetres = *scrapScaleTransform;
    }
    return fit;
}

Th2SimilarityTransform joinPartnerToThisTransform(const Th2CentrelineFit &thisFit,
                                      const QPointF &thisJoinPoint,
                                      const Th2CentrelineFit &partnerFit,
                                      const QPointF &partnerJoinPoint)
{
    // Each survey's metres frame has its own origin, so the frames are linked at the equated station.
    const QPointF joinOffset = thisFit.drawingToMetres.map(thisJoinPoint)
        - partnerFit.drawingToMetres.map(partnerJoinPoint);
    return partnerFit.drawingToMetres.then(Th2SimilarityTransform::fromTranslation(joinOffset))
        .then(thisFit.drawingToMetres.inverted());
}
}
