#pragma once

#include "../core/ProjectJoinList.h"

#include <QColor>
#include <QIcon>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QToolButton;
class QTreeView;

namespace TherionStudio
{
// Joins page of the project navigation sidebar: cross-survey equates for one projection.
class ProjectJoinsPanel final : public QWidget
{
    Q_OBJECT

public:
    // Text colours for rows joined by Therion `join` commands; chosen by the application theme.
    struct ConnectionColors
    {
        QColor automaticScrapJoin;
        QColor countedScrapJoin;
        QColor pointLineJoin;
    };

    explicit ProjectJoinsPanel(QWidget *parent = nullptr);

    void setProjectIndex(const ProjectIndexSnapshot &snapshot);
    // Whether double-click / Enter opens the second drawing's map in a separate window; the context menu offers the
    // other choice.
    void setActivationOpensSeparateWindow(bool separateWindow);
    void setConnectionColors(const ConnectionColors &colors);

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
    QStandardItem *joinOnlyItem(const ProjectJoinGroup &group, int groupIndex) const;
    // Bold marks an automatic scrap join backed by an equate or nosurvey leg (`linkedByStation`).
    void applyConnectionStyle(QStandardItem *item, ProjectJoinConnection connection, bool linkedByStation) const;
    QStringList connectionDescription(ProjectJoinConnection connection, const QVector<ProjectJoinCommand> &joins) const;
    QString linkDescription(const ProjectEquateLink &link) const;
    void openJoinOnlyScraps(const ProjectJoinGroup &group);
    void updateLegend();
    QIcon mixedJoinIcon() const;

    ProjectIndexSnapshot snapshot_;
    QVector<ProjectJoinGroup> groups_;
    QComboBox *projectionCombo_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    // Key below the list: what each row colour, font and marker means; the header button folds it away.
    QWidget *legendBox_ = nullptr;
    QToolButton *legendToggle_ = nullptr;
    QWidget *legend_ = nullptr;
    QGridLayout *legendGrid_ = nullptr;
    ConnectionColors colors_;
    QTreeView *tree_ = nullptr;
    QStandardItemModel *model_ = nullptr;
    QSortFilterProxyModel *proxyModel_ = nullptr;
    bool activationOpensSeparateWindow_ = false;
    bool contextMenuFromKeyboard_ = false;
};
}
