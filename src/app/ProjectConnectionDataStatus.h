#pragma once

#include "ProjectStationPositionService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>

namespace TherionStudio
{
// The line above the Connections list: where station positions come from and whether they are current.
class ProjectConnectionDataStatus final
{
    Q_DECLARE_TR_FUNCTIONS(ProjectConnectionDataStatus)

public:
    struct State
    {
        ProjectStationPositionService::DataSource source = ProjectStationPositionService::DataSource::None;
        QDateTime takenAt;
        bool outOfDate = false;
        bool running = false;
    };

    // `now` decides whether the date is shown with the time.
    [[nodiscard]] static QString text(const State &state, const QDateTime &now);
};
}
