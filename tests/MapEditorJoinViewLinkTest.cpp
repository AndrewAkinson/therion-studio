#include "../src/app/text_editor/map_editor/MapEditorJoinViewLink.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)
        && file.write(text.toUtf8()) == text.toUtf8().size();
}

// A scrap whose station points sit on XVI stations drawn at `unitsPerMetre`.
QString drawing(const QString &xviName, qreal unitsPerMetre)
{
    return QStringLiteral("##XTHERION## xth_me_image_insert {0.00 1 1.0} {0.00 {}} %1 0 {}\n"
                          "scrap s -projection plan\n"
                          "point 0 0 station -name 1\n"
                          "point %2 0 station -name 2\n"
                          "point %2 %3 station -name 3\n"
                          "endscrap\n")
        .arg(xviName, QString::number(10 * unitsPerMetre), QString::number(5 * unitsPerMetre));
}

QString xvi(qreal unitsPerMetre)
{
    return QStringLiteral("set XVIgrids {1 m}\n"
                          "set XVIstations {\n"
                          "  {0 0 1}\n"
                          "  {%1 0 2}\n"
                          "  {%1 %2 3}\n"
                          "}\n"
                          "set XVIgrid {0 0 %3 0 0 %3 10 10}\n")
        .arg(QString::number(10 * unitsPerMetre), QString::number(5 * unitsPerMetre), QString::number(unitsPerMetre));
}

bool near(const QPointF &left, const QPointF &right, qreal tolerance)
{
    return QLineF(left, right).length() <= tolerance;
}

// Views scroll in whole pixels on each axis, so compare centres to within 2 screen pixels.
bool centredOn(const MapEditorTab *tab, const QPointF &sourcePoint, qreal pixels = 2.0)
{
    return near(tab->mapViewCentreSourcePosition(), sourcePoint, pixels / tab->mapViewPixelsPerSourceUnit());
}

// Both views show the same real point: the second follows where the first actually is.
bool showSamePoint(const MapEditorTab *first, const MapEditorTab *second, const Th2SimilarityTransform &secondToFirst)
{
    return centredOn(first, secondToFirst.map(second->mapViewCentreSourcePosition()));
}
}

class MapEditorJoinViewLinkTest final : public QObject
{
    Q_OBJECT

private slots:
    void linksTwoEditorsAtTheJoinStation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QDir root(dir.path());
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("a.xvi")), xvi(78.74)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("b.xvi")), xvi(19.685)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("a.th2")), drawing(QStringLiteral("a.xvi"), 78.74)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("b.th2")), drawing(QStringLiteral("b.xvi"), 19.685)));

        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        QWidget host;
        host.resize(1400, 800);
        auto *layout = new QHBoxLayout(&host);
        auto *first = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), &host);
        auto *second = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), &host);
        layout->addWidget(first);
        layout->addWidget(second);
        QVERIFY(first->loadFile(root.filePath(QStringLiteral("a.th2"))));
        QVERIFY(second->loadFile(root.filePath(QStringLiteral("b.th2"))));
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        // Join: station 3 of the first drawing is equated with station 1 of the second.
        const int firstJoinLine = 5;
        const int secondJoinLine = 3;
        const QPointF firstJoin(10 * 78.74, 5 * 78.74);
        const QPointF secondJoin(0, 0);
        const Th2CentrelineFit firstFit = first->centrelineFitForSourceLine(firstJoinLine);
        const Th2CentrelineFit secondFit = second->centrelineFitForSourceLine(secondJoinLine);
        QCOMPARE(firstFit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(secondFit.stationCount, 3);
        const Th2SimilarityTransform secondToFirst =
            joinPartnerToThisTransform(firstFit, firstJoin, secondFit, secondJoin);
        QVERIFY(near(secondToFirst.map(secondJoin), firstJoin, 1e-6));

        MapEditorJoinViewLink link(first, second, secondToFirst);
        link.showFirstAt(firstJoin, first->mapViewPixelsPerSourceUnit());
        QVERIFY(centredOn(first, firstJoin));
        QVERIFY(showSamePoint(first, second, secondToFirst));
        QVERIFY(centredOn(second, secondJoin, 3.0));
        // One metre is 78.74 first units and 19.685 second units, so it spans the same pixels in both.
        QVERIFY(qAbs(second->mapViewPixelsPerSourceUnit() / first->mapViewPixelsPerSourceUnit() - 4.0) < 0.05);

        // Panning the second editor one metre east moves the first one metre east.
        second->setMapViewCentreAndScale(secondJoin + QPointF(19.685, 0), second->mapViewPixelsPerSourceUnit());
        QVERIFY(showSamePoint(first, second, secondToFirst));
        QVERIFY(centredOn(first, firstJoin + QPointF(78.74, 0), 3.0));

        // Unlinked views move independently.
        link.setEnabled(false);
        const QPointF firstBefore = first->mapViewCentreSourcePosition();
        second->setMapViewCentreAndScale(secondJoin, second->mapViewPixelsPerSourceUnit());
        QVERIFY(near(first->mapViewCentreSourcePosition(), firstBefore, 1e-6));
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorJoinViewLinkTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorJoinViewLinkTest.moc"
