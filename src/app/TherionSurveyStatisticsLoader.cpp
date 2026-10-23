#include "TherionSurveyStatisticsLoader.h"

#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

namespace TherionStudio
{
TherionSurveyStatisticsLoader::TherionSurveyStatisticsLoader(QObject *parent)
    : QObject(parent)
    , loadWatcher_(new QFutureWatcher<Loaded>(this))
{
    connect(loadWatcher_, &QFutureWatcher<Loaded>::finished, this, &TherionSurveyStatisticsLoader::handleLoadFinished);
}

void TherionSurveyStatisticsLoader::updateFromCompile(const TherionCompileResult &result)
{
    const quint64 generation = ++generation_;
    if (!result.isValid()) {
        statistics_ = TherionSurveyStatistics();
        emit statisticsChanged();
        return;
    }
    if (!result.succeeded || result.xtherionDataPath.isEmpty()) {
        markLaterCompileFailed();
        return;
    }

    loadWatcher_->setFuture(QtConcurrent::run([result, generation]() {
        Loaded loaded;
        loaded.generation = generation;
        loaded.compile = result;
        QFile file(result.xtherionDataPath);
        if (file.open(QIODevice::ReadOnly)) {
            loaded.surveys = parseTherionXtherionSurveys(QString::fromUtf8(file.readAll()));
        }
        return loaded;
    }));
}

const TherionSurveyStatistics &TherionSurveyStatisticsLoader::statistics() const
{
    return statistics_;
}

void TherionSurveyStatisticsLoader::handleLoadFinished()
{
    const Loaded loaded = loadWatcher_->result();
    if (loaded.generation != generation_) {
        return;
    }
    // Therion writes the survey tree only when the compile got far enough.
    if (loaded.surveys.isEmpty()) {
        markLaterCompileFailed();
        return;
    }

    TherionSurveyStatistics statistics;
    statistics.compile = loaded.compile;
    for (const TherionXtherionSurvey &survey : loaded.surveys) {
        statistics.surveysByFullName.insert(survey.fullName, survey);
    }
    statistics_ = statistics;
    emit statisticsChanged();
}

void TherionSurveyStatisticsLoader::markLaterCompileFailed()
{
    if (!statistics_.isAvailable() || statistics_.laterCompileFailed) {
        return;
    }
    statistics_.laterCompileFailed = true;
    emit statisticsChanged();
}
}
