#pragma once

#include "../core/ProjectJoinList.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace TherionStudio
{
// Joins page of the project navigation sidebar: cross-survey equates for one projection.
class ProjectJoinsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectJoinsPanel(QWidget *parent = nullptr);

    void setProjectIndex(const ProjectIndexSnapshot &snapshot);

signals:
    void joinRequested(const TherionStudio::ProjectJoinPair &pair);
    void sourceLocationRequested(const QString &sourceFile, int lineNumber);
    void statusMessageRequested(const QString &message);

private:
    void rebuildRows();
    void activateRow(const QModelIndex &proxyIndex);
    void showContextMenu(const QPoint &position);
    const ProjectJoinPair *pairForIndex(const QModelIndex &proxyIndex) const;
    const ProjectJoinGroup *groupForIndex(const QModelIndex &proxyIndex) const;
    QStandardItem *pairItem(const ProjectJoinPair &pair, const QString &text, int groupIndex, int pairIndex) const;
    QString unavailableReason(const ProjectJoinPair &pair) const;
    QString sideDescription(const ProjectJoinSide &side) const;

    ProjectIndexSnapshot snapshot_;
    QVector<ProjectJoinGroup> groups_;
    QComboBox *projectionCombo_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QTreeView *tree_ = nullptr;
    QStandardItemModel *model_ = nullptr;
    QSortFilterProxyModel *proxyModel_ = nullptr;
};
}
