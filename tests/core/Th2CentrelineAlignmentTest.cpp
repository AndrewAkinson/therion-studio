#include "../../src/core/Th2CentrelineAlignment.h"
#include "../../src/core/TherionSourceLogicalDocument.h"
#include "../../src/core/TherionXviParser.h"

#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool nearlyEqual(qreal left, qreal right, qreal tolerance = 1e-6)
{
    return qAbs(left - right) <= tolerance;
}

bool nearlyEqual(const QPointF &left, const QPointF &right, qreal tolerance = 1e-6)
{
    return nearlyEqual(left.x(), right.x(), tolerance) && nearlyEqual(left.y(), right.y(), tolerance);
}

TherionXviDocument xviDocument(const QString &text)
{
    TherionXviDocument document;
    if (!parseTherionXviDocumentText(text, &document)) {
        qWarning("XVI fixture did not parse");
    }
    return document;
}

const QString kXviText = QStringLiteral(
    "set XVIgrids {1 m}\n"
    "set XVIstations {\n"
    "  {0.0 0.0 1}\n"
    "  {78.74 0.0 2}\n"
    "  {78.74 157.48 3}\n"
    "}\n"
    "set XVIgrid {-100.0 -100.0 78.74 0.0 0.0 78.74 10 10}\n");
}

class Th2CentrelineAlignmentTest final : public QObject
{
    Q_OBJECT

private slots:
    void composesAndInvertsSimilarities()
    {
        const Th2SimilarityTransform rotateScale{0.0, 2.0, 1.0, 0.0}; // 90 degrees, scale 2, shift x
        QVERIFY(nearlyEqual(rotateScale.map(QPointF(1, 0)), QPointF(1, 2)));
        QVERIFY(nearlyEqual(rotateScale.scale(), 2.0));
        QVERIFY(nearlyEqual(rotateScale.inverted().map(QPointF(1, 2)), QPointF(1, 0)));
        const Th2SimilarityTransform shifted = rotateScale.then(Th2SimilarityTransform::fromTranslation(QPointF(0, 5)));
        QVERIFY(nearlyEqual(shifted.map(QPointF(1, 0)), QPointF(1, 7)));
        QVERIFY(nearlyEqual(rotateScale.then(rotateScale.inverted()).map(QPointF(3, 4)), QPointF(3, 4)));
    }

    void convertsTherionLengthUnits()
    {
        QCOMPARE(therionLengthUnitMetres(QStringLiteral("m")).value_or(0.0), 1.0);
        QCOMPARE(therionLengthUnitMetres(QStringLiteral("FT")).value_or(0.0), 0.3048);
        QVERIFY(!therionLengthUnitMetres(QStringLiteral("parsec")).has_value());
    }

    void readsXviGridScale()
    {
        const std::optional<qreal> metresPerUnit = xviMetresPerDrawingUnit(xviDocument(kXviText));
        QVERIFY(metresPerUnit.has_value());
        QVERIFY(nearlyEqual(*metresPerUnit, 1.0 / 78.74));
    }

    void recoversSimilarityFromStationPairs()
    {
        // drawing = 2 units per metre, rotated 30 degrees, shifted.
        const qreal angle = qDegreesToRadians(30.0);
        const Th2SimilarityTransform truth{0.5 * qCos(angle), 0.5 * qSin(angle), 5.0, -3.0};
        QVector<Th2StationFitPair> pairs;
        for (const QPointF &drawing : {QPointF(0, 0), QPointF(10, 0), QPointF(4, 12)}) {
            pairs.append(Th2StationFitPair{QString(), drawing, truth.map(drawing)});
        }
        qreal residual = -1.0;
        const std::optional<Th2SimilarityTransform> fit = similarityFit(pairs, &residual);
        QVERIFY(fit.has_value());
        QVERIFY(nearlyEqual(residual, 0.0));
        QVERIFY(nearlyEqual(fit->map(QPointF(7, -2)), truth.map(QPointF(7, -2))));
        QVERIFY(!similarityFit(pairs.mid(0, 1)).has_value());
    }

    void fitsScrapStationsToXviCentreline()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap other\n"
            "point 9 9 station -name 1\n"
            "endscrap\n"
            "scrap s1 -projection plan\n"
            "point 10 20 station -name 1\n"
            "point 88.74 20 station -name 2\n"
            "point 40 40 label -text 2\n"
            "point 88.74 177.48 station -name 3\n"
            "point 0 0 station -name unknown\n"
            "endscrap\n"));
        const QVector<Th2ScrapStationPoint> points = th2ScrapStationPoints(th2.commands(), 4);
        QCOMPARE(points.size(), 4);
        QCOMPARE(points.first().position, QPointF(10, 20));
        QCOMPARE(points.first().lineNumber, 5);

        const TherionXviDocument xvi = xviDocument(kXviText);
        const QVector<Th2StationFitPair> pairs = xviStationFitPairs(points, xvi);
        QCOMPARE(pairs.size(), 3);
        const Th2CentrelineFit fit = fitTh2ScrapToCentreline(pairs, xviMetresPerDrawingUnit(xvi), std::nullopt);
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(fit.stationCount, 3);
        QVERIFY(nearlyEqual(fit.rmsResidualMetres, 0.0));
        QVERIFY(nearlyEqual(fit.drawingToMetres.map(QPointF(88.74, 177.48)), QPointF(1.0, 2.0)));
    }

    void fitsScrapContainingLineWithBestXvi()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap first\n"
            "point 0 0 station -name 9\n"
            "endscrap\n"
            "scrap s1 -scale [1 m]\n"
            "point 10 20 station -name 1\n"
            "point 88.74 20 station -name 2\n"
            "endscrap\n"));
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 6), 4);
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 2), 1);
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 8), 0);
        QCOMPARE(th2StationPointAtLine(th2.commands(), 6)->position, QPointF(88.74, 20));
        QVERIFY(!th2StationPointAtLine(th2.commands(), 4).has_value());

        const TherionXviDocument unrelated = xviDocument(QStringLiteral(
            "set XVIgrids {1 m}\nset XVIstations {\n  {0 0 77}\n}\nset XVIgrid {0 0 10 0 0 10 1 1}\n"));
        const Th2CentrelineFit fit = fitTh2ScrapContainingLine(th2.commands(), 5, {unrelated, xviDocument(kXviText)});
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(fit.stationCount, 2);
        QVERIFY(nearlyEqual(fit.drawingToMetres.scale(), 1.0 / 78.74));

        const Th2CentrelineFit scaleOnly = fitTh2ScrapContainingLine(th2.commands(), 5, {});
        QCOMPARE(scaleOnly.source, Th2CentrelineFitSource::ScrapScale);
        QVERIFY(!fitTh2ScrapContainingLine(th2.commands(), 8, {}).isValid());
    }

    void usesXviGridScaleForSingleStation()
    {
        const QVector<Th2StationFitPair> pairs = {{QStringLiteral("1"), QPointF(10, 20), QPointF(3, 4)}};
        const Th2CentrelineFit fit = fitTh2ScrapToCentreline(pairs, 0.5, std::nullopt);
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(fit.stationCount, 1);
        QVERIFY(nearlyEqual(fit.drawingToMetres.map(QPointF(10, 20)), QPointF(3, 4)));
        QVERIFY(nearlyEqual(fit.drawingToMetres.map(QPointF(12, 20)), QPointF(4, 4)));
    }

    void fallsBackToScrapScale()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap s -projection plan -scale [0 0 787.40 787.40 0.0 0.0 10 10 m]\n"
            "endscrap\n"
            "scrap t -scale [2 cm]\n"
            "endscrap\n"
            "scrap u\n"
            "endscrap\n"));
        const std::optional<Th2SimilarityTransform> eightPoint = th2ScrapScaleTransform(th2.commands(), 1);
        QVERIFY(eightPoint.has_value());
        QVERIFY(nearlyEqual(eightPoint->map(QPointF(787.40, 0.0)), QPointF(10.0, 0.0)));
        const std::optional<Th2SimilarityTransform> length = th2ScrapScaleTransform(th2.commands(), 3);
        QVERIFY(length.has_value());
        QVERIFY(nearlyEqual(length->map(QPointF(100, 0)), QPointF(2.0, 0.0)));
        QVERIFY(!th2ScrapScaleTransform(th2.commands(), 5).has_value());

        const Th2CentrelineFit fit = fitTh2ScrapToCentreline({}, std::nullopt, eightPoint);
        QCOMPARE(fit.source, Th2CentrelineFitSource::ScrapScale);
        QVERIFY(!fitTh2ScrapToCentreline({}, std::nullopt, std::nullopt).isValid());
    }

    void linksPartnerDrawingAtJoinStation()
    {
        // This drawing: 78.74 units per metre. Partner: 19.685 units per metre, its own origin.
        Th2CentrelineFit thisFit;
        thisFit.source = Th2CentrelineFitSource::XviStations;
        thisFit.drawingToMetres = Th2SimilarityTransform::fromScale(1.0 / 78.74);
        Th2CentrelineFit partnerFit;
        partnerFit.source = Th2CentrelineFitSource::XviStations;
        partnerFit.drawingToMetres = Th2SimilarityTransform::fromScale(1.0 / 19.685)
                                         .then(Th2SimilarityTransform::fromTranslation(QPointF(-50, 7)));

        const QPointF thisJoin(-1832.02, 848.59);
        const QPointF partnerJoin(43.85, 12.99);
        const Th2SimilarityTransform partnerToThis = joinPartnerToThisTransform(thisFit, thisJoin, partnerFit, partnerJoin);
        QVERIFY(nearlyEqual(partnerToThis.map(partnerJoin), thisJoin, 1e-6));
        // One metre in the partner drawing is one metre in this drawing.
        const QPointF partnerMetreEast = partnerJoin + QPointF(19.685, 0.0);
        QVERIFY(nearlyEqual(partnerToThis.map(partnerMetreEast), thisJoin + QPointF(78.74, 0.0), 1e-6));
    }
};

int runTh2CentrelineAlignmentTest(int argc, char **argv)
{
    Th2CentrelineAlignmentTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "Th2CentrelineAlignmentTest.moc"
