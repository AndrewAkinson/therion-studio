#include "../src/app/ProjectJoinsPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QSignalSpy>
#include <QTreeView>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectEquateStation station(const QString &reference, const QString &namespacePath)
{
    return ProjectEquateStation{reference, reference + QLatin1Char('|') + namespacePath, namespacePath, true};
}

ProjectStationDrawingPoint drawing(const ProjectEquateStation &station, const QString &file, const QString &projection)
{
    return ProjectStationDrawingPoint{station.stationKey, file, 9, QStringLiteral("s"), QStringLiteral("survey"), projection};
}

ProjectIndexSnapshot joinSnapshot()
{
    const ProjectEquateStation a = station(QStringLiteral("1@upper"), QStringLiteral("upper"));
    const ProjectEquateStation b = station(QStringLiteral("2@lower"), QStringLiteral("lower"));
    const ProjectEquateStation c = station(QStringLiteral("3@side"), QStringLiteral("side"));
    ProjectIndexSnapshot snapshot;
    snapshot.equateLinks = {ProjectEquateLink{QStringLiteral("root.th"), 5, {a, b}},
                            ProjectEquateLink{QStringLiteral("root.th"), 6, {a, b, c}}};
    snapshot.stationDrawingPointsByStationKey[a.stationKey] = {drawing(a, QStringLiteral("upper.th2"), QStringLiteral("plan"))};
    snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("lower.th2"), QStringLiteral("plan")),
                                                               drawing(b, QStringLiteral("lower-ee.th2"), QStringLiteral("extended"))};
    snapshot.stationDrawingPointsByStationKey[c.stationKey] = {drawing(c, QStringLiteral("side-ee.th2"), QStringLiteral("extended"))};
    return snapshot;
}
}

class ProjectJoinsPanelTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsJoinsAndOpensAvailablePair()
    {
        ProjectJoinsPanel panel;
        panel.setProjectIndex(joinSnapshot());
        auto *combo = panel.findChild<QComboBox *>();
        auto *tree = panel.findChild<QTreeView *>();
        QVERIFY(combo != nullptr && tree != nullptr);
        QCOMPARE(combo->currentText(), QStringLiteral("plan"));
        QCOMPARE(combo->count(), 2);

        QAbstractItemModel *model = tree->model();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("1@upper = 2@lower"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("1@upper = 2@lower = 3@side"));
        QCOMPARE(model->rowCount(model->index(1, 0)), 3);

        QSignalSpy joinSpy(&panel, &ProjectJoinsPanel::joinRequested);
        QSignalSpy statusSpy(&panel, &ProjectJoinsPanel::statusMessageRequested);
        emit tree->activated(model->index(0, 0));
        QCOMPARE(joinSpy.count(), 1);
        const QList<QVariant> arguments = joinSpy.takeFirst();
        QCOMPARE(arguments.at(1).toBool(), false); // default: both drawings as tabs
        const ProjectJoinPair pair = arguments.at(0).value<ProjectJoinPair>();
        QCOMPARE(pair.first.drawing->sourceFile, QStringLiteral("upper.th2"));
        QCOMPARE(pair.second.drawing->sourceFile, QStringLiteral("lower.th2"));

        // 1@upper = 3@side: side is only drawn in the extended view.
        emit tree->activated(model->index(1, 0, model->index(1, 0)));
        QCOMPARE(joinSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 1);
        QVERIFY(statusSpy.takeFirst().at(0).toString().contains(QStringLiteral("3@side")));

        // The Settings option swaps double-click to open the second drawing in a separate window.
        panel.setActivationOpensSeparateWindow(true);
        emit tree->activated(model->index(0, 0));
        QCOMPARE(joinSpy.count(), 1);
        QCOMPARE(joinSpy.takeFirst().at(1).toBool(), true);
    }

    void keyboardContextMenuTargetsTheCurrentRow()
    {
        ProjectJoinsPanel panel;
        panel.resize(400, 400);
        panel.setProjectIndex(joinSnapshot());
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto *tree = panel.findChild<QTreeView *>();
        tree->setFocus();
        // Row 1 is a multi-station group: its menu has no `Open Join` (that is per pair), unlike row 0.
        tree->setCurrentIndex(tree->model()->index(1, 0));

        // Read the popup's actions, then close it so the modal menu returns.
        QStringList menuActions;
        QTimer::singleShot(0, [&menuActions]() {
            if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
                for (const QAction *action : menu->actions()) {
                    menuActions.append(action->text());
                }
                menu->close();
            }
        });
        // The Menu key / Shift+F10 sends a keyboard context-menu event to the focused view.
        QContextMenuEvent keyboardMenu(QContextMenuEvent::Keyboard, QPoint(0, 0), tree->mapToGlobal(QPoint(0, 0)));
        QCoreApplication::sendEvent(tree, &keyboardMenu);

        QVERIFY2(menuActions.contains(tr("Go to Equate in Source")), qPrintable(menuActions.join(QLatin1Char('|'))));
        QVERIFY2(!menuActions.contains(tr("Open Join")), qPrintable(menuActions.join(QLatin1Char('|'))));
    }

    void keepsProjectionAndFiltersRows()
    {
        ProjectJoinsPanel panel;
        panel.setProjectIndex(joinSnapshot());
        auto *combo = panel.findChild<QComboBox *>();
        auto *filter = panel.findChild<QLineEdit *>();
        auto *tree = panel.findChild<QTreeView *>();
        combo->setCurrentText(QStringLiteral("extended"));
        panel.setProjectIndex(joinSnapshot());
        QCOMPARE(combo->currentText(), QStringLiteral("extended"));

        filter->setText(QStringLiteral("SIDE"));
        QCOMPARE(tree->model()->rowCount(), 1);
        QCOMPARE(tree->model()->index(0, 0).data().toString(), QStringLiteral("1@upper = 2@lower = 3@side"));
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ProjectJoinsPanelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectJoinsPanelTest.moc"
