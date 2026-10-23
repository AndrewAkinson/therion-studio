#include "MainWindowStructureInfoPresenter.h"

#include "../core/ProjectStructureIndex.h"

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
        return entry.namespacePath;
    }
    if (entry.namespacePath.isEmpty()) {
        return entry.name;
    }
    return QStringLiteral("%1@%2").arg(entry.name, entry.namespacePath);
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
        if (entry->createsNamespace) {
            result.fields.append({tr("Full name"), therionFullName(*entry)});
        } else {
            result.fields.append({tr("Namespace"), tr("off")});
        }
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

    return QStringLiteral("<p><b>%1</b></p><table>%2</table>")
        .arg(presentation.heading.toHtmlEscaped(), rows);
}
}
