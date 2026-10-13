#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;

QString fixtureSource()
{
    return QStringLiteral(
        "encoding utf-8\n"                       // 1
        "scrap a -projection plan\n"             // 2
        "point 0 0 station -name a1\n"           // 3
        "point 60 0 station -name a2\n"          // 4
        "point 60 30 stalactite\n"               // 5
        "line wall\n"                            // 6
        "  0 60\n  120 60\n"                     // 7-8
        "endline\n"                              // 9
        "endscrap\n"                             // 10
        "scrap b -projection plan\n"             // 11
        "point 0 -120 station -name b1\n"        // 12
        "line border\n"                          // 13
        "  0 -120\n  60 -120\n"                  // 14-15
        "endline\n"                              // 16
        "endscrap\n");                           // 17
}

QList<QGraphicsItem *> sceneGeometryItems(QGraphicsScene *scene)
{
    QList<QGraphicsItem *> result;
    if (scene == nullptr) {
        return result;
    }
    for (QGraphicsItem *item : scene->items()) {
        if (item == nullptr) {
            continue;
        }
        bool ok = false;
        const int line = item->data(kMapSceneLineNumberRole).toInt(&ok);
        if (ok && line > 0 && !item->data(kMapSceneSelectionGatedRole).toBool()) {
            result.append(item);
        }
    }
    return result;
}

int countVisibleItems(const QList<QGraphicsItem *> &items)
{
    int count = 0;
    for (QGraphicsItem *item : items) {
        if (item != nullptr && item->isVisible()) {
            ++count;
        }
    }
    return count;
}

int countRowsWithVisibilityTooltip(const QStandardItemModel *model, const QString &tooltip, const QModelIndex &parent = QModelIndex())
{
    if (model == nullptr) {
        return 0;
    }
    int count = 0;
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex visIdx = model->index(row, kInspectorObjectVisibilityColumn, parent);
        if (visIdx.data(Qt::ToolTipRole).toString() == tooltip) {
            ++count;
        }
        const QModelIndex nameIdx = model->index(row, kInspectorObjectNameColumn, parent);
        count += countRowsWithVisibilityTooltip(model, tooltip, nameIdx);
    }
    return count;
}
}

class MapEditorBulkVisibilityTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/bulk-vis-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
    };

    bool open(Harness &harness)
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid()) {
            return false;
        }
        const QString path = harness.directory.filePath(QStringLiteral("bulk_visibility.th2"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(fixtureSource().toUtf8());
        file.close();
        harness.tab = std::make_unique<MapEditorTab>(harness.fileSystem, harness.sessionStore, CommandCatalogStore());
        harness.tab->resize(1280, 900);
        harness.tab->show();
        QString error;
        if (!harness.tab->loadFile(path, &error)) {
            qWarning() << error;
            return false;
        }
        harness.view = harness.tab->findChild<QGraphicsView *>();
        if (harness.view == nullptr) {
            return false;
        }
        QTest::qWait(50);
        harness.tab->triggerFit();
        QCoreApplication::processEvents();
        return true;
    }

private slots:
    void testBulkHideAndShowAll()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *hideAllBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsHideAllButton"));
        auto *showAllBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsShowAllButton"));
        QVERIFY(hideAllBtn != nullptr);
        QVERIFY(showAllBtn != nullptr);

        auto *objectsTree = harness.tab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
        QVERIFY(objectsTree != nullptr);
        auto *model = qobject_cast<const QStandardItemModel *>(objectsTree->model());
        QVERIFY(model != nullptr);

        QGraphicsScene *scene = harness.view->scene();
        QVERIFY(scene != nullptr);

        const QList<QGraphicsItem *> items = sceneGeometryItems(scene);
        QVERIFY(!items.isEmpty());

        // Initial state: all geometry items are visible
        const int totalItems = items.size();
        QCOMPARE(countVisibleItems(items), totalItems);
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")) > 0);
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")), 0);

        // Click Hide All
        hideAllBtn->click();
        QCoreApplication::processEvents();

        // All geometry items should now be hidden
        QCOMPARE(countVisibleItems(items), 0);
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")), 0);
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")) > 0);

        // Click Show All
        showAllBtn->click();
        QCoreApplication::processEvents();

        // All geometry items should now be visible again
        QCOMPARE(countVisibleItems(items), totalItems);
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")) > 0);
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")), 0);
    }
};

int runMapEditorBulkVisibilityTest(int argc, char **argv)
{
    MapEditorBulkVisibilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorBulkVisibilityTest.moc"
