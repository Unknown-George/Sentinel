#include "ui/ProcessModel.h"

#include <algorithm>

namespace sentinel {
namespace {
QString memorySize(quint64 size) {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = double(size);
    int unit = 0;
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : (value < 10 ? 2 : 1)) + " " + units[unit];
}
bool sameIdentity(const Process& a, const Process& b) {
    return a.pid == b.pid && a.startTime == b.startTime;
}
} // namespace

ProcessModel::ProcessModel(QObject* parent) : QAbstractTableModel(parent) {}
int ProcessModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(items.size());
}
int ProcessModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 5;
}
QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole)
        return int(section > 0 && section < 4 ? Qt::AlignRight | Qt::AlignVCenter
                                             : Qt::AlignLeft | Qt::AlignVCenter);
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QStringList{"Process", "PID", "CPU %", "Memory", "User"}.value(section);
    return {};
}
QVariant ProcessModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= items.size() ||
        index.column() < 0 || index.column() >= 5)
        return {};
    const auto& item = items[index.row()];
    const int column = index.column();
    if (role == Qt::TextAlignmentRole)
        return int(column > 0 && column < 4 ? Qt::AlignRight | Qt::AlignVCenter
                                         : Qt::AlignLeft | Qt::AlignVCenter);
    if (role == Qt::ToolTipRole)
        return item.path;
    if (role == Qt::UserRole) {
        switch (column) {
        case 0:
            return item.name;
        case 1:
            return item.pid;
        case 2:
            return item.cpuPercent.value_or(-1.);
        case 3:
            return QVariant::fromValue(item.residentBytes);
        case 4:
            return item.user;
        }
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (column) {
    case 0:
        return item.name;
    case 1:
        return item.pid;
    case 2:
        return item.cpuPercent ? QString::number(*item.cpuPercent, 'f', 1) + "%" : QString::fromUtf8("—");
    case 3:
        return memorySize(item.residentBytes);
    case 4:
        return item.user;
    }
    return {};
}
void ProcessModel::replace(QVector<Process> processes) {
    std::sort(processes.begin(), processes.end(),
              [](const Process& a, const Process& b) { return a.pid < b.pid; });
    processes.erase(std::unique(processes.begin(), processes.end(),
                                [](const Process& a, const Process& b) { return a.pid == b.pid; }),
                    processes.end());

    // A reused PID is a new process. Removing it also invalidates an old selection.
    auto survives = [&processes](const Process& item) {
        const auto found = std::lower_bound(processes.cbegin(), processes.cend(), item.pid,
                                            [](const Process& p, int pid) { return p.pid < pid; });
        return found != processes.cend() && sameIdentity(item, *found);
    };
    for (int row = int(items.size()) - 1; row >= 0;) {
        if (survives(items[row])) {
            --row;
            continue;
        }
        const int last = row;
        while (row >= 0 && !survives(items[row]))
            --row;
        beginRemoveRows({}, row + 1, last);
        items.remove(row + 1, last - row);
        endRemoveRows();
    }

    for (int row = 0; row < processes.size();) {
        if (row < items.size() && sameIdentity(items[row], processes[row])) {
            ++row;
            continue;
        }
        const int first = row;
        const int nextPid = row < items.size() ? items[row].pid : -1;
        while (row < processes.size() && processes[row].pid != nextPid)
            ++row;
        beginInsertRows({}, first, row - 1);
        items.insert(first, row - first, Process{});
        for (int index = first; index < row; ++index)
            items[index] = processes[index];
        endInsertRows();
    }

    // Coalesce consecutive changes into one notification instead of repainting and
    // re-sorting the proxy once for every process on every collection interval.
    int firstChanged = -1, firstColumn = 5, lastColumn = -1;
    auto notifyChanges = [&](int lastRow) {
        if (firstChanged >= 0)
            emit dataChanged(index(firstChanged, firstColumn), index(lastRow, lastColumn),
                             {Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole});
        firstChanged = -1;
        firstColumn = 5;
        lastColumn = -1;
    };
    for (int row = 0; row < processes.size(); ++row) {
        const auto& old = items[row];
        const auto& next = processes[row];
        int first = 5, last = -1;
        auto changed = [&](bool changed, int column) {
            if (changed) {
                first = std::min(first, column);
                last = std::max(last, column);
            }
        };
        changed(old.name != next.name, 0);
        changed(old.cpuPercent != next.cpuPercent, 2);
        changed(old.residentBytes != next.residentBytes, 3);
        changed(old.user != next.user, 4);
        if (old.path != next.path) {
            first = 0;
            last = 4;
        }
        if (last < 0) {
            notifyChanges(row - 1);
            continue;
        }
        if (firstChanged < 0)
            firstChanged = row;
        firstColumn = std::min(firstColumn, first);
        lastColumn = std::max(lastColumn, last);
        items[row] = next;
    }
    notifyChanges(int(processes.size()) - 1);
}
ProcessFilter::ProcessFilter(QObject* parent) : QSortFilterProxyModel(parent) {
    // The custom filter below depends on several columns, including user changes.
    // This also tells Qt to reconsider filtering after a change outside column 0.
    setFilterKeyColumn(-1);
}
bool ProcessFilter::filterAcceptsRow(int row, const QModelIndex& parent) const {
    const auto expression = filterRegularExpression();
    for (int column : {0, 1, 4})
        if (sourceModel()->index(row, column, parent).data().toString().contains(expression))
            return true;
    return false;
}
} // namespace sentinel
