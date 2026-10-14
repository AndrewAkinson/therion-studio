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

// How Therion `join` commands connect the drawings of two surveys in one projection.
enum class ProjectJoinConnection
{
    None,
    // A scrap join without `-count`: Therion joins the closest scrap ends.
    AutomaticScrapJoin,
    // A scrap join with `-count N`.
    CountedScrapJoin,
    // Joins of points and lines.
    PointLineJoin,
    // More than one kind between the same surveys: an input error.
    MixedJoins,
};

// Two stations of one equate that belong to different surveys, seen in one projection.
struct ProjectJoinPair
{
    ProjectJoinSide first;
    ProjectJoinSide second;
    ProjectJoinConnection connection = ProjectJoinConnection::None;
    QVector<ProjectJoinCommand> joins;

    [[nodiscard]] bool isAvailable() const
    {
        return first.status == ProjectJoinSideStatus::Drawn && second.status == ProjectJoinSideStatus::Drawn;
    }
};

struct ProjectJoinGroup
{
    ProjectEquateLink link;
    QVector<ProjectJoinPair> pairs;
    // Set instead of `link` for a cross-survey `join` with no equate or `nosurvey` leg between its surveys.
    std::optional<ProjectJoinCommand> joinOnly;
    ProjectJoinConnection joinOnlyConnection = ProjectJoinConnection::None;
    QVector<ProjectStructureEntry> joinOnlyScraps;
};

// Projections that draw at least one equated station: plan, extended, then elevations by angle, then others.
[[nodiscard]] QStringList projectJoinProjections(const ProjectIndexSnapshot &snapshot);

// One group per cross-survey equate or `nosurvey` leg (a multi-station equate yields one pair per two surveys), each
// pair classified by the `join` commands between its surveys in `projection`, followed by one group per join-only
// connection in `projection`.
[[nodiscard]] QVector<ProjectJoinGroup> projectJoinGroups(const ProjectIndexSnapshot &snapshot,
                                                          const QString &projection);
}
