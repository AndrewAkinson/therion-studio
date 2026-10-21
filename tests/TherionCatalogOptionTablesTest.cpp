#include "../src/app/text_editor/TextEditorValidationCatalog.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/TherionSourceValidator.h"

#include <QDir>

#include <QtTest/QtTest>

using namespace TherionStudio;

// The bundled catalog follows Therion's option tables (plans/VALIDATION_ACCURACY_PLAN.md, V2; GitHub issue #50).
class TherionCatalogOptionTablesTest final : public QObject
{
    Q_OBJECT

private slots:
    void bundledCatalogFollowsTherionOptionTables()
    {
        const QString catalogPath = QDir(QCoreApplication::applicationDirPath())
                                        .absoluteFilePath(QStringLiteral("../resources/therion_command_catalog.json"));
        const TherionSourceValidationCatalog catalog =
            validationCatalogFromCommandCatalog(CommandCatalogStore::fromFile(catalogPath).catalogObject());
        QVERIFY(!catalog.commandNames.isEmpty());

        TherionSourceDocumentMetadata mapMetadata;
        mapMetadata.sourceType = TherionSourceDocumentType::TherionMap;
        const TherionSourceValidationResult drawing = TherionSourceValidator::validate(
            QStringLiteral("scrap s1 -proj plan -author 2024 \"A Person\"\n"
                           "  point 0 0 label -scale l -align t -text [Main Chamber] -x-size 2\n"
                           "endscrap\n"),
            catalog,
            mapMetadata);
        for (const TherionSourceDiagnostic &diagnostic : drawing.diagnostics) {
            QVERIFY2(!diagnostic.code.contains(QStringLiteral("option")), qPrintable(diagnostic.message));
        }

        TherionSourceDocumentMetadata sourceMetadata;
        sourceMetadata.sourceType = TherionSourceDocumentType::TherionSource;
        const TherionSourceValidationResult map = TherionSourceValidator::validate(
            QStringLiteral("survey cave\n  map m1 -colour red\n  endmap\nendsurvey\n"), catalog, sourceMetadata);
        bool colourReported = false;
        for (const TherionSourceDiagnostic &diagnostic : map.diagnostics) {
            colourReported = colourReported || (diagnostic.code == QStringLiteral("unknown-option")
                                                && diagnostic.currentText.contains(QStringLiteral("-colour")));
        }
        QVERIFY(colourReported);

        TherionSourceDocumentMetadata configMetadata;
        configMetadata.sourceType = TherionSourceDocumentType::TherionConfig;
        const TherionSourceValidationResult config = TherionSourceValidator::validate(
            QStringLiteral("source cave.th\n"
                           "export map -proj plan -layout local -layout-language en -o \"cave.pdf\"\n"
                           "export model -enable walls -wall-source maps -o cave.lox\n"
                           "export map -print-symbols\n"),
            catalog,
            configMetadata);
        QStringList unknownOptions;
        for (const TherionSourceDiagnostic &diagnostic : config.diagnostics) {
            if (diagnostic.code == QStringLiteral("unknown-option")) {
                unknownOptions.append(diagnostic.currentText);
            }
        }
        // Only the Therion command-line switch is not an export option.
        QCOMPARE(unknownOptions.size(), 1);
        QVERIFY(unknownOptions.first().contains(QStringLiteral("-print-symbols")));
    }
};

QTEST_GUILESS_MAIN(TherionCatalogOptionTablesTest)

#include "TherionCatalogOptionTablesTest.moc"
