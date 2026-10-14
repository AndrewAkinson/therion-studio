#pragma once

#include "ProjectStructureIndex.h"

#include <QStringList>
#include <QVector>

#include <optional>

namespace TherionStudio
{
enum class ProjectJoinSideStatus
{
    Drawn,
    Unresolved,
    NotDrawn,
    NotDrawnInProjection,
};

struct ProjectJoinSide
{
    ProjectEquateStation station;
    ProjectJoinSideStatus status = ProjectJoinSideStatus::NotDrawn;
    std::optional<ProjectStationDrawingPoint> drawing;
};

// Two stations of one equate that belong to different surveys, seen in one projection.
struct ProjectJoinPair
{
    ProjectJoinSide first;
    ProjectJoinSide second;

    [[nodiscard]] bool isAvailable() const
    {
        return first.status == ProjectJoinSideStatus::Drawn && second.status == ProjectJoinSideStatus::Drawn;
    }
};

struct ProjectJoinGroup
{
    ProjectEquateLink link;
    QVector<ProjectJoinPair> pairs;
};

// Projections that draw at least one equated station: plan, extended, then elevations by angle, then others.
[[nodiscard]] QStringList projectJoinProjections(const ProjectIndexSnapshot &snapshot);

// One group per cross-survey equate; a multi-station equate yields one pair per two surveys.
[[nodiscard]] QVector<ProjectJoinGroup> projectJoinGroups(const ProjectIndexSnapshot &snapshot,
                                                          const QString &projection);
}
