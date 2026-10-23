#pragma once

#include "TherionCompileResult.h"

#include "../core/TherionXtherionData.h"

#include <QHash>
#include <QObject>

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
// Survey statistics of the last good compile, from the .xtherion.dat it wrote.
struct TherionSurveyStatistics
{
    // The compile the figures come from; invalid before the first good compile.
    TherionCompileResult compile;
    QHash<QString, TherionXtherionSurvey> surveysByFullName;
    // A later compile failed or wrote no survey tree, so the figures are older.
    bool laterCompileFailed = false;

    [[nodiscard]] bool isAvailable() const { return compile.isValid(); }
};

// Reads .xtherion.dat off the UI thread after each compile and keeps the last good figures.
class TherionSurveyStatisticsLoader final : public QObject
{
    Q_OBJECT

public:
    explicit TherionSurveyStatisticsLoader(QObject *parent = nullptr);

    // An invalid result (project closed) clears the figures.
    void updateFromCompile(const TherionCompileResult &result);

    [[nodiscard]] const TherionSurveyStatistics &statistics() const;

signals:
    void statisticsChanged();

private:
    struct Loaded
    {
        quint64 generation = 0;
        TherionCompileResult compile;
        QVector<TherionXtherionSurvey> surveys;
    };

    void handleLoadFinished();
    void markLaterCompileFailed();

    quint64 generation_ = 0;
    TherionSurveyStatistics statistics_;
    QFutureWatcher<Loaded> *loadWatcher_ = nullptr;
};
}
