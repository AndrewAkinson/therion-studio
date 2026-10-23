#pragma once

#include <QCoreApplication>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct ProjectStructureEntry;

// Text of the info box under the Survey and Map Structure trees.
class MainWindowStructureInfoPresenter final
{
    Q_DECLARE_TR_FUNCTIONS(MainWindowStructureInfoPresenter)

public:
    struct Field
    {
        QString label;
        QString value;
    };

    struct Presentation
    {
        QString heading;
        QVector<Field> fields;
        QString sourceLocation;
        QString prompt;
    };

    // Link target of the definition location in html().
    static QString sourceLinkTarget();

    // entry is null when no survey, map or scrap row is selected.
    static Presentation present(const ProjectStructureEntry *entry, const QString &sourceDisplayPath);
    static QString html(const Presentation &presentation);
};

// Name shown for a structure entry; unnamed scraps get a translated label.
QString structureEntryDisplayName(const ProjectStructureEntry &entry);
}
