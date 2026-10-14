#include "ProjectJoinsPanel.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
constexpr int GroupIndexRole = Qt::UserRole + 1;
constexpr int PairIndexRole = Qt::UserRole + 2;
constexpr int RowKeyRole = Qt::UserRole + 3;

QString stationReferences(const QVector<ProjectEquateStation> &stations)
{
    QStringList references;
    references.reserve(stations.size());
    for (const ProjectEquateStation &station : stations) {
        references.append(station.reference);
    }
    return references.join(QStringLiteral(" = "));
}

QString groupKey(const ProjectEquateLink &link)
{
    return QStringLiteral("%1:%2").arg(link.sourceFile, QString::number(link.lineNumber));
}

QString fileLocation(const QString &sourceFile, int lineNumber)
{
    return QStringLiteral("%1:%2").arg(QFileInfo(sourceFile).fileName(), QString::number(lineNumber));
}
}

ProjectJoinsPanel::ProjectJoinsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *projectionRow = new QHBoxLayout;
    projectionRow->setContentsMargins(0, 0, 0, 0);
    projectionRow->setSpacing(6);
    auto *projectionLabel = new QLabel(tr("View:"), this);
    projectionCombo_ = new QComboBox(this);
    projectionCombo_->setToolTip(tr("Scrap projection whose drawings are joined"));
    projectionLabel->setBuddy(projectionCombo_);
    projectionRow->addWidget(projectionLabel);
    projectionRow->addWidget(projectionCombo_, 1);
    layout->addLayout(projectionRow);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText(tr("Filter by survey or station"));
    filterEdit_->setClearButtonEnabled(true);
    layout->addWidget(filterEdit_);

    model_ = new QStandardItemModel(this);
    proxyModel_ = new QSortFilterProxyModel(this);
    proxyModel_->setSourceModel(model_);
    proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel_->setRecursiveFilteringEnabled(true);

    tree_ = new QTreeView(this);
    tree_->setMinimumWidth(0);
    tree_->setModel(proxyModel_);
    tree_->setRootIsDecorated(true);
    tree_->setAnimated(true);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setAlternatingRowColors(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->header()->hide();
    layout->addWidget(tree_, 1);

    connect(projectionCombo_, &QComboBox::currentTextChanged, this, [this]() { rebuildRows(); });
    connect(filterEdit_, &QLineEdit::textChanged, proxyModel_, &QSortFilterProxyModel::setFilterFixedString);
    connect(tree_, &QTreeView::activated, this, &ProjectJoinsPanel::activateRow);
    connect(tree_, &QTreeView::customContextMenuRequested, this, &ProjectJoinsPanel::showContextMenu);
}

void ProjectJoinsPanel::setProjectIndex(const ProjectIndexSnapshot &snapshot)
{
    snapshot_ = snapshot;
    const QString currentProjection = projectionCombo_->currentText();
    const QStringList projections = projectJoinProjections(snapshot_);
    {
        const QSignalBlocker blocker(projectionCombo_);
        projectionCombo_->clear();
        projectionCombo_->addItems(projections);
        const int keptIndex = projections.indexOf(currentProjection);
        projectionCombo_->setCurrentIndex(keptIndex >= 0 ? keptIndex : (projections.isEmpty() ? -1 : 0));
    }
    rebuildRows();
}

void ProjectJoinsPanel::rebuildRows()
{
    // Keep expansion and selection across background index refreshes.
    QSet<QString> expandedKeys;
    for (int row = 0; row < proxyModel_->rowCount(); ++row) {
        const QModelIndex index = proxyModel_->index(row, 0);
        if (tree_->isExpanded(index)) {
            expandedKeys.insert(index.data(RowKeyRole).toString());
        }
    }
    const QString currentKey = tree_->currentIndex().data(RowKeyRole).toString();

    groups_ = projectJoinGroups(snapshot_, projectionCombo_->currentText());
    model_->clear();
    for (int groupIndex = 0; groupIndex < groups_.size(); ++groupIndex) {
        const ProjectJoinGroup &group = groups_.at(groupIndex);
        const QString text = stationReferences(group.link.stations);
        QStandardItem *groupItem = nullptr;
        if (group.pairs.size() == 1) {
            groupItem = pairItem(group.pairs.first(), text, groupIndex, 0);
        } else {
            groupItem = new QStandardItem(text);
            groupItem->setData(groupIndex, GroupIndexRole);
            groupItem->setToolTip(tr("Equate: %1").arg(fileLocation(group.link.sourceFile, group.link.lineNumber)));
            for (int pairIndex = 0; pairIndex < group.pairs.size(); ++pairIndex) {
                const ProjectJoinPair &pair = group.pairs.at(pairIndex);
                groupItem->appendRow(pairItem(pair,
                                              stationReferences({pair.first.station, pair.second.station}),
                                              groupIndex,
                                              pairIndex));
            }
        }
        groupItem->setData(groupKey(group.link), RowKeyRole);
        model_->appendRow(groupItem);
    }

    for (int row = 0; row < proxyModel_->rowCount(); ++row) {
        const QModelIndex index = proxyModel_->index(row, 0);
        if (expandedKeys.contains(index.data(RowKeyRole).toString())) {
            tree_->expand(index);
        }
        if (!currentKey.isEmpty() && index.data(RowKeyRole).toString() == currentKey) {
            tree_->setCurrentIndex(index);
        }
    }
}

QStandardItem *ProjectJoinsPanel::pairItem(const ProjectJoinPair &pair,
                                           const QString &text,
                                           int groupIndex,
                                           int pairIndex) const
{
    auto *item = new QStandardItem(text);
    item->setData(groupIndex, GroupIndexRole);
    item->setData(pairIndex, PairIndexRole);
    item->setData(QStringLiteral("%1/%2").arg(groupIndex).arg(pairIndex), RowKeyRole);
    const ProjectEquateLink &link = groups_.at(groupIndex).link;
    QStringList toolTip = {tr("Equate: %1").arg(fileLocation(link.sourceFile, link.lineNumber)),
                           sideDescription(pair.first),
                           sideDescription(pair.second)};
    if (!pair.isAvailable()) {
        item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
    }
    item->setToolTip(toolTip.join(QLatin1Char('\n')));
    return item;
}

QString ProjectJoinsPanel::sideDescription(const ProjectJoinSide &side) const
{
    switch (side.status) {
    case ProjectJoinSideStatus::Drawn:
        return tr("%1: %2 (scrap %3)")
            .arg(side.station.reference,
                 fileLocation(side.drawing->sourceFile, side.drawing->lineNumber),
                 side.drawing->scrapName);
    case ProjectJoinSideStatus::Unresolved:
        return tr("%1: station not found in any centreline").arg(side.station.reference);
    case ProjectJoinSideStatus::NotDrawn:
        return tr("%1: no station point in a drawing that Therion reads").arg(side.station.reference);
    case ProjectJoinSideStatus::NotDrawnInProjection:
        return tr("%1: not drawn in the %2 view").arg(side.station.reference, projectionCombo_->currentText());
    }
    return QString();
}

QString ProjectJoinsPanel::unavailableReason(const ProjectJoinPair &pair) const
{
    return pair.first.status != ProjectJoinSideStatus::Drawn ? sideDescription(pair.first)
                                                             : sideDescription(pair.second);
}

const ProjectJoinGroup *ProjectJoinsPanel::groupForIndex(const QModelIndex &proxyIndex) const
{
    const QVariant groupIndex = proxyIndex.data(GroupIndexRole);
    if (!groupIndex.isValid() || groupIndex.toInt() < 0 || groupIndex.toInt() >= groups_.size()) {
        return nullptr;
    }
    return &groups_.at(groupIndex.toInt());
}

const ProjectJoinPair *ProjectJoinsPanel::pairForIndex(const QModelIndex &proxyIndex) const
{
    const ProjectJoinGroup *group = groupForIndex(proxyIndex);
    const QVariant pairIndex = proxyIndex.data(PairIndexRole);
    if (group == nullptr || !pairIndex.isValid() || pairIndex.toInt() >= group->pairs.size()) {
        return nullptr;
    }
    return &group->pairs.at(pairIndex.toInt());
}

void ProjectJoinsPanel::activateRow(const QModelIndex &proxyIndex)
{
    const ProjectJoinPair *pair = pairForIndex(proxyIndex);
    if (pair == nullptr) {
        tree_->setExpanded(proxyIndex, !tree_->isExpanded(proxyIndex));
        return;
    }
    if (!pair->isAvailable()) {
        emit statusMessageRequested(unavailableReason(*pair));
        return;
    }
    emit joinRequested(*pair);
}

void ProjectJoinsPanel::showContextMenu(const QPoint &position)
{
    const QModelIndex proxyIndex = tree_->indexAt(position);
    const ProjectJoinGroup *group = groupForIndex(proxyIndex);
    if (group == nullptr) {
        return;
    }

    // Copies: a background index refresh may rebuild groups_ while the menu is open.
    const ProjectEquateLink link = group->link;
    const std::optional<ProjectJoinPair> pair = pairForIndex(proxyIndex) != nullptr
        ? std::optional<ProjectJoinPair>(*pairForIndex(proxyIndex))
        : std::nullopt;
    QMenu menu(this);
    if (pair.has_value()) {
        QAction *openJoin = menu.addAction(tr("Open Join"));
        openJoin->setEnabled(pair->isAvailable());
        connect(openJoin, &QAction::triggered, this, [this, pair]() { emit joinRequested(*pair); });
    }
    QAction *goToEquate = menu.addAction(tr("Go to Equate in Source"));
    connect(goToEquate, &QAction::triggered, this, [this, link]() {
        emit sourceLocationRequested(link.sourceFile, link.lineNumber);
    });
    if (pair.has_value()) {
        menu.addSeparator();
        for (const ProjectJoinSide *side : {&pair->first, &pair->second}) {
            if (!side->drawing.has_value()) {
                continue;
            }
            const ProjectStationDrawingPoint drawing = *side->drawing;
            QAction *openDrawing = menu.addAction(tr("Open %1").arg(QFileInfo(drawing.sourceFile).fileName()));
            connect(openDrawing, &QAction::triggered, this, [this, drawing]() {
                emit sourceLocationRequested(drawing.sourceFile, drawing.lineNumber);
            });
        }
    }
    menu.exec(tree_->viewport()->mapToGlobal(position));
}
}
