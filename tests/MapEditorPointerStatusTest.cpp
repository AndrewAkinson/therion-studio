// Status bar pointer coordinates in the map editor.
//
// The text is the magnifier's coordinate readout, so both always agree. It must
// follow the pointer whether or not the magnifier is shown, and clear when the
// pointer leaves the map.

#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMouseEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include <memory>

using namespace TherionStudio;

namespace
{
void pumpEvents()
{
    for (int index = 0; index < 8; ++index) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

bool writeFixture(const QDir &directory, QString *filePath)
{
    *filePath = directory.filePath(QStringLiteral("pointer.th2"));
    QFile file(*filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    // Drawing units differ from view units so a missing conversion would show.
    file.write("encoding utf-8\n"
               "##XTHERION## xth_me_area_adjust -20 -40 60 40\n"
               "##XTHERION## xth_me_area_zoom_to 100\n"
               "\nscrap pointer -projection plan\n"
               "point 0 0 station -name A\n"
               "point 40 20 station -name B\n"
               "endscrap\n");
    return true;
}

void sendMouseMove(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseMove,
                      QPointF(position),
                      QPointF(viewport->mapToGlobal(position)),
                      Qt::NoButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}
}

class MapEditorPointerStatusTest final : public QObject
{
    Q_OBJECT

private slots:
    void coordinateTextUsesOneDecimalAndDotSeparator();
    void statusFollowsPointerAndClearsOnLeave_data();
    void statusFollowsPointerAndClearsOnLeave();
};

void MapEditorPointerStatusTest::coordinateTextUsesOneDecimalAndDotSeparator()
{
    const QLocale previousLocale;
    QLocale::setDefault(QLocale(QLocale::Czech));
    const QString text = MapEditorTab::pointerCoordinateText(QPointF(1234.54, -67.84));
    QLocale::setDefault(previousLocale);

    QCOMPARE(text, QStringLiteral("x 1234.5  y -67.8"));
}

void MapEditorPointerStatusTest::statusFollowsPointerAndClearsOnLeave_data()
{
    QTest::addColumn<bool>("magnifierEnabled");

    QTest::newRow("magnifier shown") << true;
    QTest::newRow("magnifier hidden") << false;
}

void MapEditorPointerStatusTest::statusFollowsPointerAndClearsOnLeave()
{
    QFETCH(bool, magnifierEnabled);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QString filePath;
    QVERIFY(writeFixture(QDir(temporaryDirectory.path()), &filePath));

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    mapTab->setMagnifierEnabled(magnifierEnabled);
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    QVERIFY(mapView != nullptr);
    QWidget *viewport = mapView->viewport();
    QVERIFY(mapTab->pointerStatusText().isEmpty());

    QSignalSpy statusSpy(mapTab, &MapEditorTab::pointerStatusChanged);
    const QPoint position = viewport->rect().center() + QPoint(17, -11);
    sendMouseMove(viewport, position);

    const QString expected = MapEditorTab::pointerCoordinateText(
        mapTab->sourcePointFromScenePositionForTest(mapView->mapToScene(position)));
    QTRY_COMPARE(mapTab->pointerStatusText(), expected);
    QVERIFY(statusSpy.count() >= 1);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(viewport, &leaveEvent);
    QVERIFY(mapTab->pointerStatusText().isEmpty());
}

int runMapEditorPointerStatusTest(int argc, char **argv)
{
    MapEditorPointerStatusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorPointerStatusTest.moc"
