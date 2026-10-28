#pragma once

#include <QDialog>
#include <QColor>
#include <QString>
#include "../core/MapAppearanceSettings.h"

class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;

namespace TherionStudio
{
class MainWindowSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Settings
    {
        QString applicationLanguage = QStringLiteral("system");
        QString therionExecutablePath;
        QString defaultTextEditorMode = QStringLiteral("raw");
        bool mapPointerCoordinatesInStatusBarEnabled = false;
        bool mapXviStationNamesInStatusBarEnabled = false;
        bool automaticProjectValidationEnabled = false;
        bool mapAltitudeUnderlayEnabled = false;
        MapAppearanceSettings mapAppearance;
        bool joinOpensSeparateWindow = false;
        QString connectionPositionRunMode = QStringLiteral("ask");
        bool troubleshootingLogsEnabled = false;
    };

    explicit MainWindowSettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void browseTherionExecutable();
    void autoDetectTherionExecutable();
    void openDiagnosticLogFolder();
    void clearDiagnosticLogs();
    void selectComboData(QComboBox *combo, const QString &data, const QString &fallbackData);

    QComboBox *languageCombo_ = nullptr;
    QLineEdit *therionExecutableEdit_ = nullptr;
    QPushButton *therionExecutableBrowseButton_ = nullptr;
    QPushButton *therionExecutableAutoDetectButton_ = nullptr;
    QComboBox *defaultTextEditorModeCombo_ = nullptr;
    QComboBox *xviCenterlineColorModeCombo_ = nullptr;
    QCheckBox *mapPointerCoordinatesCheckBox_ = nullptr;
    QCheckBox *mapXviStationNamesCheckBox_ = nullptr;
    QCheckBox *automaticProjectValidationCheckBox_ = nullptr;
    QCheckBox *mapAltitudeUnderlayCheckBox_ = nullptr;
    QSpinBox *mapAltitudeUnderlayStrengthSpin_ = nullptr;
    QPushButton *xviCenterlineColorButton_ = nullptr;
    QDoubleSpinBox *xviCenterlineWidthSpin_ = nullptr;
    QColor xviCenterlineColor_;
    QCheckBox *joinOpensSeparateWindowCheckBox_ = nullptr;
    QComboBox *connectionPositionRunModeCombo_ = nullptr;
    QCheckBox *troubleshootingLogsCheckBox_ = nullptr;
    QPushButton *openLogFolderButton_ = nullptr;
    QPushButton *clearLogsButton_ = nullptr;
};
}
