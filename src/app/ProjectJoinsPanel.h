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
    // Whether double-click / Enter opens the second drawing's map in a separate window; the context menu offers the
    // other choice.
    void setActivationOpensSeparateWindow(bool separateWindow);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void joinRequested(const TherionStudio::ProjectJoinPair &pair, bool separateWindow);
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
    bool activationOpensSeparateWindow_ = false;
    bool contextMenuFromKeyboard_ = false;
};
}
