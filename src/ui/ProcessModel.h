#pragma once

#include "core/Types.h"
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

namespace sentinel {
class ProcessModel final : public QAbstractTableModel {
  public:
    explicit ProcessModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    // Keep surviving processes in place so selection and scrolling survive live samples.
    void replace(QVector<Process> processes);
    QVector<Process> items;
};

class ProcessFilter final : public QSortFilterProxyModel {
  public:
    explicit ProcessFilter(QObject* parent = nullptr);

  protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
};
} // namespace sentinel
