#include "../src/app/MainWindowStructureInfoPresenter.h"
#include "../src/core/ProjectStructureIndex.h"
#include "../src/core/TherionXtherionData.h"

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
    void notesSurveyStatisticsBeforeCompile();
    void presentsSurveyStatistics();
    void notesSurveyMissingFromCompile();
    void notesOutOfDateStatistics();
    void omitsStatisticsForMaps();
};

MainWindowStructureInfoPresenter::SurveyStatisticsInput compiledInput(const TherionXtherionSurvey *survey)
{
    MainWindowStructureInfoPresenter::SurveyStatisticsInput input;
    input.available = true;
    input.survey = survey;
    return input;
}

void MainWindowStructureInfoPresenterTest::notesSurveyStatisticsBeforeCompile()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(presentation.statistics.isEmpty());
    QCOMPARE(presentation.notes, QStringList{QStringLiteral("Survey statistics appear after a successful compile.")});
}

void MainWindowStructureInfoPresenterTest::presentsSurveyStatistics()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    TherionXtherionSurvey survey;
    survey.title = QStringLiteral("Inner Series");
    survey.statistics = QStringLiteral("length: 15.00m\nnumber of shots: 2");
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"), compiledInput(&survey));

    QCOMPARE(fieldValue(presentation, QStringLiteral("Title")), QStringLiteral("Inner Series"));
    QCOMPARE(presentation.statistics, QStringList({QStringLiteral("length: 15.00m"), QStringLiteral("number of shots: 2")}));
    QVERIFY(presentation.notes.isEmpty());
    QVERIFY(MainWindowStructureInfoPresenter::html(presentation).contains(QStringLiteral("length: 15.00m<br>number of shots: 2")));
}

void MainWindowStructureInfoPresenterTest::notesSurveyMissingFromCompile()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"), compiledInput(nullptr));

    QVERIFY(presentation.statistics.isEmpty());
    QCOMPARE(presentation.notes, QStringList{QStringLiteral("This survey is not in the last successful compile.")});
}

void MainWindowStructureInfoPresenterTest::notesOutOfDateStatistics()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    TherionXtherionSurvey survey;
    survey.statistics = QStringLiteral("length: 15.00m");
    auto input = compiledInput(&survey);
    input.sourcesChanged = true;
    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList{QStringLiteral("Out of date because sources changed after the last compile.")});

    input.laterCompileFailed = true;
    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList{QStringLiteral("The last compile failed, so these figures are from an earlier compile.")});
}

void MainWindowStructureInfoPresenterTest::omitsStatisticsForMaps()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QStringLiteral("cave"));
    TherionXtherionSurvey survey;
    survey.statistics = QStringLiteral("length: 15.00m");
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QString(), compiledInput(&survey));

    QVERIFY(presentation.statistics.isEmpty());
    QVERIFY(presentation.notes.isEmpty());
}

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

    // Therion still names a -namespace off survey child.parent.
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("flat.cave"));
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
