#include "../src/app/MainWindowStructureInfoPresenter.h"
#include "../src/core/ProjectStructureIndex.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectStructureEntry makeEntry(const QString &category, const QString &name, const QString &namespacePath)
{
    ProjectStructureEntry entry;
    entry.category = category;
    entry.name = name;
    entry.namespacePath = namespacePath;
    entry.sourceFile = QStringLiteral("/project/cave/cave.th");
    entry.lineNumber = 12;
    return entry;
}

QString fieldValue(const MainWindowStructureInfoPresenter::Presentation &presentation, const QString &label)
{
    for (const MainWindowStructureInfoPresenter::Field &field : presentation.fields) {
        if (field.label == label) {
            return field.value;
        }
    }
    return QString();
}

class MainWindowStructureInfoPresenterTest : public QObject
{
    Q_OBJECT

private slots:
    void presentsSurvey();
    void presentsSurveyWithNamespaceOff();
    void presentsMapWithFullName();
    void presentsScrapAtTopLevel();
    void presentsUnnamedScrapWithoutFullName();
    void promptsWithoutSelection();
    void promptsForOtherRows();
    void omitsLocationWithoutLine();
    void htmlEscapesAndLinksSource();
};

void MainWindowStructureInfoPresenterTest::presentsSurvey()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QCOMPARE(presentation.heading, QStringLiteral("Survey inner"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("inner.cave"));
    QCOMPARE(presentation.sourceLocation, QStringLiteral("cave/cave.th, line 12"));
    QVERIFY(presentation.prompt.isEmpty());
}

void MainWindowStructureInfoPresenterTest::presentsSurveyWithNamespaceOff()
{
    ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("flat"), QStringLiteral("cave"));
    entry.createsNamespace = false;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(fieldValue(presentation, QStringLiteral("Full name")).isEmpty());
    QCOMPARE(fieldValue(presentation, QStringLiteral("Namespace")), QStringLiteral("off"));
}

void MainWindowStructureInfoPresenterTest::presentsMapWithFullName()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QCOMPARE(presentation.heading, QStringLiteral("Map MP1"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("MP1@inner.cave"));
}

void MainWindowStructureInfoPresenterTest::presentsScrapAtTopLevel()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("SP1"), QString());
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave.th2"));

    QCOMPARE(presentation.heading, QStringLiteral("Scrap SP1"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("SP1"));
}

void MainWindowStructureInfoPresenterTest::presentsUnnamedScrapWithoutFullName()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("Unnamed Scrap"), QStringLiteral("cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave.th2"));

    QCOMPARE(presentation.heading, QStringLiteral("Scrap Unnamed Scrap"));
    QVERIFY(presentation.fields.isEmpty());
}

void MainWindowStructureInfoPresenterTest::promptsWithoutSelection()
{
    const auto presentation = MainWindowStructureInfoPresenter::present(nullptr, QString());

    QVERIFY(!presentation.prompt.isEmpty());
    QVERIFY(presentation.heading.isEmpty());
    QVERIFY(presentation.fields.isEmpty());
}

void MainWindowStructureInfoPresenterTest::promptsForOtherRows()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Stations"), QStringLiteral("1"), QStringLiteral("cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(!presentation.prompt.isEmpty());
    QVERIFY(presentation.heading.isEmpty());
}

void MainWindowStructureInfoPresenterTest::omitsLocationWithoutLine()
{
    ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QString());
    entry.lineNumber = 0;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(presentation.sourceLocation.isEmpty());
    QVERIFY(!MainWindowStructureInfoPresenter::html(presentation).contains(QStringLiteral("<a ")));
}

void MainWindowStructureInfoPresenterTest::htmlEscapesAndLinksSource()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("a<b"), QString());
    const QString html = MainWindowStructureInfoPresenter::html(
        MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th")));

    QVERIFY(html.contains(QStringLiteral("a&lt;b")));
    QVERIFY(!html.contains(QStringLiteral("a<b")));
    QVERIFY(html.contains(QStringLiteral("href=\"%1\"").arg(MainWindowStructureInfoPresenter::sourceLinkTarget())));
    QVERIFY(html.contains(QStringLiteral("cave/cave.th, line 12")));
}
}

int runMainWindowStructureInfoPresenterTest(int argc, char **argv)
{
    MainWindowStructureInfoPresenterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowStructureInfoPresenterTest.moc"
