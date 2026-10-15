#include "TherionStationPositionExport.h"

#include "ProjectStructureIndex.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionSourceReferenceResolver.h"

#include <QDir>

namespace TherionStudio
{
namespace
{
// Therion splits arguments on spaces, so a path is quoted; forward slashes work on every platform.
QString configPathArgument(const QString &path)
{
    return QStringLiteral("\"%1\"").arg(QDir::fromNativeSeparators(path));
}
}

QStringList therionConfigSourcePaths(const QString &configPath, const QString &configText)
{
    QStringList paths;
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(configText);
    for (const TherionSourceLogicalCommand &command : document.commands()) {
        if (command.metadata.commandName != QStringLiteral("source")) {
            continue;
        }
        const QString path = resolveTherionSourceReferencePath(configPath, therionSourceReferencePathToken(command));
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

QString therionStationPositionExportConfig(const QStringList &sourcePaths, const QString &outputFileName)
{
    QString config = QStringLiteral("encoding utf-8\n");
    for (const QString &path : sourcePaths) {
        config += QStringLiteral("source %1\n").arg(configPathArgument(path));
    }
    config += QStringLiteral("export database -fmt sql -o %1\n").arg(configPathArgument(outputFileName));
    return config;
}

QString therionStationPositionQuery()
{
    return QStringLiteral("SELECT STATION.NAME, SURVEY.FULL_NAME, STATION.X, STATION.Y, STATION.Z "
                          "FROM STATION JOIN SURVEY ON SURVEY.ID = STATION.SURVEY_ID");
}

QHash<QString, Th2StationPosition> therionStationPositionsFromRows(const QVector<QStringList> &rows)
{
    QHash<QString, Th2StationPosition> positions;
    for (const QStringList &row : rows) {
        if (row.size() < 5) {
            continue;
        }
        const QString name = row.at(0).trimmed();
        if (name.isEmpty() || name == QStringLiteral("-") || name == QStringLiteral(".")) {
            continue;
        }
        bool xOk = false;
        bool yOk = false;
        bool zOk = false;
        const Th2StationPosition position{row.at(2).toDouble(&xOk), row.at(3).toDouble(&yOk), row.at(4).toDouble(&zOk)};
        if (xOk && yOk && zOk) {
            positions.insert(ProjectStructureIndex::stationKey(name, row.at(1).trimmed()), position);
        }
    }
    return positions;
}
}
