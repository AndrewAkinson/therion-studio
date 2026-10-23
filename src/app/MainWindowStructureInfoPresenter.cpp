#include "MainWindowStructureInfoPresenter.h"

#include "../core/ProjectStructureIndex.h"
#include "../core/TherionXtherionData.h"

namespace TherionStudio
{
namespace
{
bool isUnnamedScrap(const ProjectStructureEntry &entry)
{
    return entry.category == QStringLiteral("Scraps") && entry.name == QStringLiteral("Unnamed Scrap");
}

// Therion full name: innermost to outermost, objects as name@survey.parent.
QString therionFullName(const ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Surveys")) {
        return ProjectStructureIndex::surveyFullName(entry);
    }
    if (entry.namespacePath.isEmpty()) {
        return entry.name;
    }
    return QStringLiteral("%1@%2").arg(entry.name, entry.namespacePath);
}

void appendSurveyStatistics(MainWindowStructureInfoPresenter::Presentation *result,
                            const MainWindowStructureInfoPresenter::SurveyStatisticsInput &statistics)
{
    if (!statistics.available) {
        result->notes.append(MainWindowStructureInfoPresenter::tr("Survey statistics appear after a successful compile."));
        return;
    }
    if (statistics.survey == nullptr) {
        result->notes.append(MainWindowStructureInfoPresenter::tr("This survey is not in the last successful compile."));
    } else {
        if (!statistics.survey->title.isEmpty()) {
            result->fields.append({MainWindowStructureInfoPresenter::tr("Title"), statistics.survey->title});
        }
        result->statistics = statistics.survey->statistics.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    if (statistics.laterCompileFailed) {
        result->notes.append(MainWindowStructureInfoPresenter::tr(
            "The last compile failed, so these figures are from an earlier compile."));
    } else if (statistics.sourcesChanged) {
        result->notes.append(MainWindowStructureInfoPresenter::tr(
            "Out of date because sources changed after the last compile."));
    }
}
}

QString structureEntryDisplayName(const ProjectStructureEntry &entry)
{
    return isUnnamedScrap(entry) ? QObject::tr("Unnamed Scrap") : entry.name;
}

QString MainWindowStructureInfoPresenter::sourceLinkTarget()
{
    return QStringLiteral("structure-info:source");
}

MainWindowStructureInfoPresenter::Presentation MainWindowStructureInfoPresenter::present(
    const ProjectStructureEntry *entry,
    const QString &sourceDisplayPath)
{
    return present(entry, sourceDisplayPath, SurveyStatisticsInput());
}

MainWindowStructureInfoPresenter::Presentation MainWindowStructureInfoPresenter::present(
    const ProjectStructureEntry *entry,
    const QString &sourceDisplayPath,
    const SurveyStatisticsInput &statistics)
{
    Presentation result;
    const bool supported = entry != nullptr
        && (entry->category == QStringLiteral("Surveys")
            || entry->category == QStringLiteral("Maps")
            || entry->category == QStringLiteral("Scraps"));
    if (!supported) {
        result.prompt = tr("Select a survey, map or scrap to see its details.");
        return result;
    }

    const QString name = structureEntryDisplayName(*entry);
    if (entry->category == QStringLiteral("Surveys")) {
        result.heading = tr("Survey %1").arg(name);
        result.fields.append({tr("Full name"), therionFullName(*entry)});
        if (!entry->createsNamespace) {
            result.fields.append({tr("Namespace"), tr("off")});
        }
        appendSurveyStatistics(&result, statistics);
    } else {
        result.heading = entry->category == QStringLiteral("Maps")
            ? tr("Map %1").arg(name)
            : tr("Scrap %1").arg(name);
        if (!isUnnamedScrap(*entry)) {
            result.fields.append({tr("Full name"), therionFullName(*entry)});
        }
    }

    if (!sourceDisplayPath.isEmpty() && entry->lineNumber > 0) {
        result.sourceLocation = tr("%1, line %2").arg(sourceDisplayPath).arg(entry->lineNumber);
    }
    return result;
}

QString MainWindowStructureInfoPresenter::html(const Presentation &presentation)
{
    if (!presentation.prompt.isEmpty()) {
        return QStringLiteral("<p>%1</p>").arg(presentation.prompt.toHtmlEscaped());
    }

    QString rows;
    const auto appendRow = [&rows](const QString &label, const QString &valueHtml) {
        rows += QStringLiteral("<tr><td style=\"padding-right:8px\">%1</td><td>%2</td></tr>")
                    .arg(label.toHtmlEscaped(), valueHtml);
    };
    for (const Field &field : presentation.fields) {
        appendRow(field.label, field.value.toHtmlEscaped());
    }
    if (!presentation.sourceLocation.isEmpty()) {
        appendRow(tr("Defined in"),
                  QStringLiteral("<a href=\"%1\">%2</a>")
                      .arg(sourceLinkTarget(), presentation.sourceLocation.toHtmlEscaped()));
    }

    QString html = QStringLiteral("<p><b>%1</b></p><table>%2</table>")
                       .arg(presentation.heading.toHtmlEscaped(), rows);
    if (!presentation.statistics.isEmpty()) {
        QStringList lines;
        for (const QString &line : presentation.statistics) {
            lines.append(line.toHtmlEscaped());
        }
        html += QStringLiteral("<p>%1</p>").arg(lines.join(QStringLiteral("<br>")));
    }
    for (const QString &note : presentation.notes) {
        html += QStringLiteral("<p><i>%1</i></p>").arg(note.toHtmlEscaped());
    }
    return html;
}
}
