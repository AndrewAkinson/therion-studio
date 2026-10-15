#include "../src/app/text_editor/map_editor/MapEditorJoinOutline.h"
#include "../src/app/text_editor/map_editor/MapEditorJoinPairs.h"
#include "../src/app/text_editor/map_editor/MapEditorJoinViewLink.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointer>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTextBlock>
#include <QTextCursor>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>

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
                          "line wall\n"
                          "  0 0\n"
                          "  %2 %3\n"
                          "endline\n"
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

// Both views show the same real point. Each view rounds its centre to whole pixels on both axes, so two
// views can differ by up to about 2.8 pixels.
bool showSamePoint(const MapEditorTab *first, const MapEditorTab *second, const Th2SimilarityTransform &secondToFirst)
{
    return centredOn(first, secondToFirst.map(second->mapViewCentreSourcePosition()), 3.0);
}
}

class MapEditorJoinTest final : public QObject
{
    Q_OBJECT

private slots:
    // Two editors side by side; station 3 of the first drawing is equated with station 1 of the second.
    void init()
    {
        QVERIFY(dir_.isValid());
        const QDir root(dir_.path());
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("a.xvi")), xvi(78.74)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("b.xvi")), xvi(19.685)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("a.th2")), drawing(QStringLiteral("a.xvi"), 78.74)));
        QVERIFY(writeTextFile(root.filePath(QStringLiteral("b.th2")), drawing(QStringLiteral("b.xvi"), 19.685)));

        host_ = std::make_unique<QWidget>();
        host_->resize(1400, 800);
        auto *layout = new QHBoxLayout(host_.get());
        first_ = new MapEditorTab(fileSystem_, sessionStore_, CommandCatalogStore(), host_.get());
        second_ = new MapEditorTab(fileSystem_, sessionStore_, CommandCatalogStore(), host_.get());
        layout->addWidget(first_);
        layout->addWidget(second_);
        QVERIFY(first_->loadFile(root.filePath(QStringLiteral("a.th2"))));
        QVERIFY(second_->loadFile(root.filePath(QStringLiteral("b.th2"))));
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));

        const Th2CentrelineFit firstFit = first_->centrelineFitForSourceLine(kFirstJoinLine);
        const Th2CentrelineFit secondFit = second_->centrelineFitForSourceLine(kSecondJoinLine);
        QCOMPARE(firstFit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(secondFit.stationCount, 3);
        QCOMPARE(first_->stationPointSourcePosition(kFirstJoinLine).value_or(QPointF()), kFirstJoin);
        QCOMPARE(second_->stationPointSourcePosition(kSecondJoinLine).value_or(QPointF(-1, -1)), kSecondJoin);
        secondToFirst_ = joinPartnerToThisTransform(firstFit, kFirstJoin, secondFit, kSecondJoin);
        QVERIFY(near(secondToFirst_.map(kSecondJoin), kFirstJoin, 1e-6));
    }

    void cleanup()
    {
        host_.reset();
    }

    void linksTwoEditorsAtTheJoinStation()
    {
        MapEditorJoinViewLink link(first_, second_, secondToFirst_);
        link.showFirstAt(kFirstJoin, first_->mapViewPixelsPerSourceUnit());
        QVERIFY(centredOn(first_, kFirstJoin));
        QVERIFY(showSamePoint(first_, second_, secondToFirst_));
        QVERIFY(centredOn(second_, kSecondJoin, 3.0));
        // One metre is 78.74 first units and 19.685 second units, so it spans the same pixels in both.
        QVERIFY(qAbs(second_->mapViewPixelsPerSourceUnit() / first_->mapViewPixelsPerSourceUnit() - 4.0) < 0.05);

        // Panning the second editor one metre east moves the first to the same place. Each view rounds to whole
        // pixels, so check the panned view against its target and the other view against the panned view.
        second_->setMapViewCentreAndScale(kSecondJoin + QPointF(19.685, 0), second_->mapViewPixelsPerSourceUnit());
        QVERIFY(centredOn(second_, kSecondJoin + QPointF(19.685, 0)));
        QVERIFY(showSamePoint(first_, second_, secondToFirst_));
    }

    void showsPartnerOutlineAlignedAtTheJoin()
    {
        MapEditorJoinOutline *outline = MapEditorJoinOutline::attach(first_, second_, secondToFirst_);
        QVERIFY(outline != nullptr);
        // Attaching the same partner again replaces the outline rather than stacking a second one.
        outline = MapEditorJoinOutline::attach(first_, second_, secondToFirst_);
        QCOMPARE(first_->findChildren<MapEditorJoinOutline *>(Qt::FindDirectChildrenOnly).size(), 1);

        MapEditorJoinOutlineItem *item = outline->item();
        QVERIFY(item != nullptr);
        QVERIFY(item->scene() == first_->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"))->scene());
        QVERIFY(!item->outlinePath().isEmpty());
        QCOMPARE(item->acceptedMouseButtons(), Qt::NoButton);

        // The partner's join station lands on this drawing's join station.
        const QPointF partnerStationScene = second_->mapSourceToSceneTransform().map(kSecondJoin);
        QVERIFY(near(item->mapToScene(partnerStationScene), first_->mapSourceToSceneTransform().map(kFirstJoin), 1e-6));

        // A full refresh of the host deletes scene items; the outline is rebuilt and re-added.
        const QPointer<MapEditorJoinOutlineItem> originalItem(item);
        QVERIFY(first_->loadFile(QDir(dir_.path()).filePath(QStringLiteral("a.th2"))));
        QTRY_VERIFY(originalItem.isNull());
        QTRY_VERIFY(outline->item() != nullptr
                    && outline->item()->scene() == first_->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"))->scene());

        outline->setOutlineVisible(false);
        QVERIFY(!outline->item()->isVisible());

        // Closing the partner removes the outline.
        const QPointer<MapEditorJoinOutline> guard(outline);
        delete second_;
        second_ = nullptr;
        QTRY_VERIFY(guard.isNull());
    }

    void followsEditsToThePartnerDrawing()
    {
        MapEditorJoinOutline *outline = MapEditorJoinOutline::attach(first_, second_, secondToFirst_);
        QVERIFY(outline != nullptr && outline->item() != nullptr);
        const int elementsBefore = outline->item()->outlinePath().elementCount();

        // Type a new wall into the partner, well outside its current bounds, as a user would in the source pane.
        auto *sourceEditor = second_->findChild<TextEditorTab *>()->findChild<QPlainTextEdit *>();
        QVERIFY(sourceEditor != nullptr);
        QTextCursor cursor(sourceEditor->document()->findBlockByLineNumber(5)); // before endscrap
        cursor.insertText(QStringLiteral("line wall\n  -400 -400\n  -300 -450\nendline\n"));
        QTRY_VERIFY(outline->item() != nullptr && outline->item()->outlinePath().elementCount() > elementsBefore);

        // The partner's scene scaling changed with its bounds; the join stations must still coincide.
        const QPointF partnerStationScene = second_->mapSourceToSceneTransform().map(kSecondJoin);
        QVERIFY(near(outline->item()->mapToScene(partnerStationScene),
                     first_->mapSourceToSceneTransform().map(kFirstJoin),
                     1e-6));
    }

    void followsMapVertexMovesInThePartnerDrawing()
    {
        MapEditorJoinOutline *outline = MapEditorJoinOutline::attach(first_, second_, secondToFirst_);
        QVERIFY(outline != nullptr && outline->item() != nullptr);
        const QRectF boundsBefore = outline->item()->outlinePath().boundingRect();

        // Drag the wall's end vertex on the partner's map, as a user would. For a plain wall the editor skips
        // its own scene refresh, so the outline must follow the source change itself.
        const QPointF wallEnd(10 * 19.685, 5 * 19.685);
        second_->testRecordLineAreaVertexMove(kSecondWallLine, QStringLiteral("line"), 1, wallEnd, wallEnd + QPointF(300, 200));
        QVERIFY(second_->text().contains(QStringLiteral("  496.85 298.425")));
        QTRY_VERIFY(outline->item() != nullptr && outline->item()->outlinePath().boundingRect() != boundsBefore);

        const QPointF partnerStationScene = second_->mapSourceToSceneTransform().map(kSecondJoin);
        QVERIFY(near(outline->item()->mapToScene(partnerStationScene),
                     first_->mapSourceToSceneTransform().map(kFirstJoin),
                     1e-6));
    }

    void opensLinksAndClosesAJoinPair()
    {
        // A line that is not a station point cannot be placed; nothing is created.
        QVERIFY(!openMapEditorJoinPair(first_, 2, second_, kSecondJoinLine));
        QVERIFY(mapEditorJoinPairsFor(first_).isEmpty());

        QVERIFY(openMapEditorJoinPair(first_, kFirstJoinLine, second_, kSecondJoinLine));
        // Opening the same join again replaces it.
        QVERIFY(openMapEditorJoinPair(first_, kFirstJoinLine, second_, kSecondJoinLine));
        QVERIFY(mapEditorHasJoinLink(first_) && mapEditorHasJoinLink(second_));
        const QVector<MapEditorJoinPair> firstPairs = mapEditorJoinPairsFor(first_);
        const QVector<MapEditorJoinPair> secondPairs = mapEditorJoinPairsFor(second_);
        QCOMPARE(firstPairs.size(), 1);
        QCOMPARE(secondPairs.size(), 1);
        QVERIFY(firstPairs.first().partner == second_);
        QVERIFY(firstPairs.first().link != nullptr && firstPairs.first().link == secondPairs.first().link);
        QVERIFY(firstPairs.first().outline != nullptr && secondPairs.first().outline != nullptr);

        // Both views open zoomed onto the join, about 30 m across the first view. Views settle once Qt has laid
        // out their scrollbars, so wait for the event loop.
        QTRY_VERIFY(centredOn(first_, kFirstJoin));
        QTRY_VERIFY(showSamePoint(first_, second_, secondToFirst_));
        const qreal metresAcross = first_->mapViewportPixelWidth() / first_->mapViewPixelsPerSourceUnit() / 78.74;
        QVERIFY2(metresAcross > 25.0 && metresAcross < 35.0, qPrintable(QString::number(metresAcross)));

        // The pair's link keeps the views together.
        second_->setMapViewCentreAndScale(kSecondJoin + QPointF(19.685, 0), second_->mapViewPixelsPerSourceUnit());
        QTRY_VERIFY(showSamePoint(first_, second_, secondToFirst_));

        // An icon set before the separate map window opens is applied when it opens, and updates while open.
        QPixmap linkPixmap(16, 16);
        linkPixmap.fill(Qt::darkGreen);
        second_->setMapPaneWindowIcon(QIcon(linkPixmap));

        // Opening with a separate window shows the second drawing's map there and keeps the views linked.
        QVERIFY(!second_->isMapPaneDetached());
        QVERIFY(openMapEditorJoinPair(first_, kFirstJoinLine, second_, kSecondJoinLine, true));
        QVERIFY(second_->isMapPaneDetached());
        QWidget *mapWindow = second_->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"))->window();
        QVERIFY(mapWindow != host_.get());
        QVERIFY(!mapWindow->windowIcon().isNull());
        second_->setMapPaneWindowIcon(QIcon());
        QVERIFY(mapWindow->windowIcon().cacheKey() != QIcon(linkPixmap).cacheKey());
        QTRY_VERIFY(showSamePoint(first_, second_, secondToFirst_));
        QCOMPARE(mapEditorJoinPairsFor(first_).size(), 1);

        closeMapEditorJoinPair(second_, first_);
        QVERIFY(!mapEditorHasJoinLink(first_));
        QVERIFY(mapEditorJoinPairsFor(first_).isEmpty());
        QVERIFY(mapEditorJoinPairsFor(second_).isEmpty());
        QVERIFY(first_->findChildren<MapEditorJoinViewLink *>().isEmpty());
    }

private:
    static constexpr int kFirstJoinLine = 5;
    static constexpr int kSecondJoinLine = 3;
    static constexpr int kSecondWallLine = 6;
    const QPointF kFirstJoin = QPointF(10 * 78.74, 5 * 78.74);
    const QPointF kSecondJoin = QPointF(0, 0);

    QTemporaryDir dir_;
    QtFileSystem fileSystem_;
    FakeSessionStore sessionStore_;
    std::unique_ptr<QWidget> host_;
    MapEditorTab *first_ = nullptr;
    MapEditorTab *second_ = nullptr;
    Th2SimilarityTransform secondToFirst_;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorJoinTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorJoinTest.moc"
