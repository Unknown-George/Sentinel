#include "ui/ProcessModel.h"
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <gtest/gtest.h>

namespace sentinel {
namespace {
Process process(int pid, double cpu = 0., quint64 start = 1) {
    Process result;
    result.pid = pid;
    result.startTime = start;
    result.name = QString("Process %1").arg(pid);
    result.path = QString("/Applications/Process%1").arg(pid);
    result.user = "test-user";
    result.cpuPercent = cpu;
    result.residentBytes = 1024 * pid;
    return result;
}
} // namespace

TEST(ProcessModel, KeepsSurvivingIndexesAndSelectionAcrossChurn) {
    ProcessModel model;
    model.replace({process(10), process(20), process(30)});
    QPersistentModelIndex selected(model.index(1, 0));
    QItemSelectionModel selection(&model);
    selection.setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    int resetCount = 0;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { ++resetCount; });

    // Incoming collector order is immaterial; existing processes keep their identity.
    model.replace({process(40), process(20, 25.), process(5), process(30)});
    ASSERT_TRUE(selected.isValid());
    EXPECT_EQ(selected.data().toString(), "Process 20");
    EXPECT_EQ(selection.currentIndex().data().toString(), "Process 20");
    EXPECT_TRUE(selection.isSelected(selected));
    EXPECT_EQ(model.items[0].pid, 5);
    EXPECT_EQ(model.items[1].pid, 20);
    EXPECT_DOUBLE_EQ(*model.items[1].cpuPercent, 25.);
    EXPECT_EQ(resetCount, 0);
}

TEST(ProcessModel, AReusedPidInvalidatesTheOldProcessIndex) {
    ProcessModel model;
    model.replace({process(10, 5., 100), process(20)});
    QPersistentModelIndex oldProcess(model.index(0, 0));
    QPersistentModelIndex survivor(model.index(1, 0));

    model.replace({process(10, 0., 200), process(20)});
    EXPECT_FALSE(oldProcess.isValid());
    EXPECT_TRUE(survivor.isValid());
    EXPECT_EQ(survivor.data().toString(), "Process 20");
    EXPECT_EQ(model.items[0].startTime, 200u);
}

TEST(ProcessModel, CoalescesUpdatesAndDoesNotNotifyForIdenticalSamples) {
    ProcessModel model;
    model.replace({process(10), process(20), process(30)});
    int changes = 0;
    int firstRow = -1, lastRow = -1, firstColumn = -1, lastColumn = -1;
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
                     [&](const QModelIndex& first, const QModelIndex& last, const QList<int>&) {
                         ++changes;
                         firstRow = first.row();
                         lastRow = last.row();
                         firstColumn = first.column();
                         lastColumn = last.column();
                     });
    model.replace({process(30), process(10), process(20)});
    EXPECT_EQ(changes, 0);

    model.replace({process(10, 12.), process(20, 24.), process(30, 36.)});
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(firstRow, 0);
    EXPECT_EQ(lastRow, 2);
    EXPECT_EQ(firstColumn, 2);
    EXPECT_EQ(lastColumn, 2);
}

TEST(ProcessModel, ProxySelectionTracksTheProcessWhenCpuSortChanges) {
    ProcessModel model;
    model.replace({process(10, 80.), process(20, 20.), process(30, 1.)});
    ProcessFilter proxy;
    proxy.setSourceModel(&model);
    proxy.setSortRole(Qt::UserRole);
    proxy.sort(2, Qt::DescendingOrder);
    QItemSelectionModel selection(&proxy);
    const QModelIndex selected = proxy.mapFromSource(model.index(1, 0));
    selection.setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    model.replace({process(5, 150.), process(10, 1.), process(20, 100.), process(30, 2.)});
    EXPECT_EQ(selection.currentIndex().data().toString(), "Process 20");
    EXPECT_EQ(selection.currentIndex().row(), 1);
    EXPECT_TRUE(selection.isSelected(selection.currentIndex()));
    EXPECT_EQ(proxy.index(0, 1).data().toInt(), 5);
    EXPECT_EQ(proxy.index(1, 1).data().toInt(), 20);

    proxy.setFilterFixedString("30");
    EXPECT_EQ(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.index(0, 1).data().toInt(), 30);
}

TEST(ProcessModel, HandlesEmptyListsAndRemovesEveryDepartedProcess) {
    ProcessModel model;
    model.replace({});
    EXPECT_EQ(model.rowCount(), 0);
    model.replace({process(10), process(20)});
    QPersistentModelIndex departed(model.index(0, 0));
    model.replace({});
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_FALSE(departed.isValid());
    model.replace({process(30)});
    EXPECT_EQ(model.index(0, 1).data().toInt(), 30);
}

TEST(ProcessModel, RefiltersWhenOnlyTheProcessUserChanges) {
    ProcessModel model;
    Process item = process(10);
    item.user = "alice";
    model.replace({item});
    ProcessFilter proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterFixedString("alice");
    ASSERT_EQ(proxy.rowCount(), 1);

    item.user = "bob";
    model.replace({item});
    EXPECT_EQ(proxy.rowCount(), 0);

    item.user = "alice";
    model.replace({item});
    EXPECT_EQ(proxy.rowCount(), 1);
}
} // namespace sentinel
