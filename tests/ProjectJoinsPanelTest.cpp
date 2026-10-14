#include "../src/app/ProjectJoinsPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
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
        const ProjectJoinPair pair = joinSpy.takeFirst().at(0).value<ProjectJoinPair>();
        QCOMPARE(pair.first.drawing->sourceFile, QStringLiteral("upper.th2"));
        QCOMPARE(pair.second.drawing->sourceFile, QStringLiteral("lower.th2"));

        // 1@upper = 3@side: side is only drawn in the extended view.
        emit tree->activated(model->index(1, 0, model->index(1, 0)));
        QCOMPARE(joinSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 1);
        QVERIFY(statusSpy.takeFirst().at(0).toString().contains(QStringLiteral("3@side")));
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
