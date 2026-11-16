#include "core/HistoryStore.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <gtest/gtest.h>
#include <sqlite3.h>

namespace sentinel {
namespace {
Snapshot sample(qint64 ms) {
    Snapshot s;
    s.timestamp = QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc());
    s.cpuPercent = 27.5;
    s.memoryTotal = 16000000000ULL;
    s.memoryUsed = 6000000000ULL;
    s.memoryValid = true;
    s.downloadBytesPerSec = 4096.0;
    s.uploadBytesPerSec = 512.0;
    s.volumes.push_back({"Macintosh HD", "/", "apfs", 1000000000000ULL, 400000000000ULL, false});
    return s;
}
} // namespace
TEST(HistoryStore, PersistsMeasuredValuesAcrossReopening) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("history.sqlite");
    const auto time = QDateTime::currentMSecsSinceEpoch();
    {
        HistoryStore store(path);
        ASSERT_TRUE(store.isOpen()) << store.error().toStdString();
        ASSERT_TRUE(store.append(sample(time))) << store.error().toStdString();
    }
    HistoryStore store(path);
    const auto points = store.query(time - 1);
    ASSERT_EQ(points.size(), 1);
    const auto& p = points.front();
    EXPECT_EQ(p.timestampMs, time);
    ASSERT_TRUE(p.cpuPercent);
    EXPECT_DOUBLE_EQ(*p.cpuPercent, 27.5);
    EXPECT_EQ(p.memoryUsed, 6000000000ULL);
    EXPECT_EQ(p.memoryTotal, 16000000000ULL);
    ASSERT_TRUE(p.downloadBytesPerSec);
    EXPECT_DOUBLE_EQ(*p.downloadBytesPerSec, 4096.0);
    ASSERT_TRUE(p.uploadBytesPerSec);
    EXPECT_DOUBLE_EQ(*p.uploadBytesPerSec, 512.0);
}
TEST(HistoryStore, MissingMeasurementsRemainNullRatherThanZero) {
    QTemporaryDir dir;
    HistoryStore store(dir.filePath("history.sqlite"));
    auto s = sample(QDateTime::currentMSecsSinceEpoch());
    s.cpuPercent.reset();
    s.downloadBytesPerSec.reset();
    s.uploadBytesPerSec.reset();
    ASSERT_TRUE(store.append(s));
    const auto points = store.query(0);
    ASSERT_EQ(points.size(), 1);
    EXPECT_FALSE(points.front().cpuPercent);
    EXPECT_FALSE(points.front().downloadBytesPerSec);
    EXPECT_FALSE(points.front().uploadBytesPerSec);
}
TEST(HistoryStore, FiltersByTimestampAndReturnsBoundedChronologicalPoints) {
    QTemporaryDir dir;
    HistoryStore store(dir.filePath("history.sqlite"));
    const auto base = QDateTime::currentMSecsSinceEpoch() - 100000;
    // Arrival order must not determine chart order.
    for (int i = 99; i >= 0; --i)
        ASSERT_TRUE(store.append(sample(base + i * 1000)));
    const auto points = store.query(base + 20000, 12);
    ASSERT_FALSE(points.empty());
    ASSERT_LE(points.size(), 12);
    for (qsizetype i = 0; i < points.size(); ++i) {
        EXPECT_GE(points[i].timestampMs, base + 20000);
        if (i)
            EXPECT_LT(points[i - 1].timestampMs, points[i].timestampMs);
    }
}
TEST(HistoryStore, RetentionAndClearRemoveOldMeasurements) {
    QTemporaryDir dir;
    HistoryStore store(dir.filePath("history.sqlite"));
    const auto now = QDateTime::currentMSecsSinceEpoch();
    ASSERT_TRUE(store.append(sample(now - 9LL * 86400000)));
    ASSERT_TRUE(store.append(sample(now)));
    ASSERT_TRUE(store.prune(7));
    auto points = store.query(0);
    ASSERT_EQ(points.size(), 1);
    EXPECT_EQ(points.front().timestampMs, now);
    ASSERT_TRUE(store.clear());
    EXPECT_TRUE(store.query(0).empty());
}
TEST(HistoryStore, UnopenablePathIsReportedAndOperationsFailSafely) {
    QTemporaryDir dir;
    QFile blocker(dir.filePath("file-is-not-a-directory"));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    HistoryStore store(blocker.fileName() + "/history.sqlite");
    EXPECT_FALSE(store.isOpen());
    EXPECT_FALSE(store.error().isEmpty());
    EXPECT_FALSE(store.append(sample(QDateTime::currentMSecsSinceEpoch())));
    EXPECT_TRUE(store.query(0).empty());
}
TEST(HistoryStore, RefusesNewerDatabaseWithoutChangingItsSchemaVersion) {
    QTemporaryDir dir;
    const auto path = dir.filePath("future.sqlite");
    sqlite3* database = nullptr;
    ASSERT_EQ(sqlite3_open(path.toUtf8().constData(), &database), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(database, "PRAGMA user_version=2", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(database);
    {
        HistoryStore store(path);
        EXPECT_FALSE(store.isOpen());
        EXPECT_TRUE(store.error().contains("newer"));
    }
    ASSERT_EQ(sqlite3_open(path.toUtf8().constData(), &database), SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database, "PRAGMA user_version", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 2);
    sqlite3_finalize(statement);
    sqlite3_close(database);
}
} // namespace sentinel
