#include "HistoryStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <memory>
#include <sqlite3.h>

namespace sentinel {
namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* value = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &value, nullptr);
    return Statement(value, sqlite3_finalize);
}
void bindOptional(sqlite3_stmt* s, int index, std::optional<double> value) {
    if (value && std::isfinite(*value))
        sqlite3_bind_double(s, index, *value);
    else
        sqlite3_bind_null(s, index);
}
std::optional<double> optionalColumn(sqlite3_stmt* s, int col) {
    if (sqlite3_column_type(s, col) == SQLITE_NULL)
        return std::nullopt;
    return sqlite3_column_double(s, col);
}
} // namespace

HistoryStore::HistoryStore(const QString& path) {
    if (path != ":memory:")
        QDir().mkpath(QFileInfo(path).absolutePath());
    if (sqlite3_open_v2(path.toUtf8().constData(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                        nullptr) != SQLITE_OK) {
        error_ = db_ ? QString::fromUtf8(sqlite3_errmsg(db_)) : "Cannot allocate database";
        if (db_)
            sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    sqlite3_busy_timeout(db_, 1500);
    if (path != ":memory:")
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    bool newerSchema = false;
    {
        auto version = prepare(db_, "PRAGMA user_version");
        newerSchema =
            version && sqlite3_step(version.get()) == SQLITE_ROW && sqlite3_column_int(version.get(), 0) > 1;
    }
    if (newerSchema) {
        error_ = "History database was created by a newer Sentinel version.";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    const char* schema = "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;"
                         "CREATE TABLE IF NOT EXISTS samples ("
                         "timestamp_ms INTEGER PRIMARY KEY, cpu REAL, memory_used INTEGER NOT NULL,"
                         "memory_total INTEGER NOT NULL, download REAL, upload REAL,"
                         "disk_used INTEGER NOT NULL, disk_total INTEGER NOT NULL);"
                         "PRAGMA user_version=1;";
    char* error = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &error) != SQLITE_OK) {
        error_ = QString::fromUtf8(error ? error : sqlite3_errmsg(db_));
        sqlite3_free(error);
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
HistoryStore::~HistoryStore() {
    if (db_)
        sqlite3_close(db_);
}

bool HistoryStore::append(const Snapshot& snapshot) {
    if (!db_ || !snapshot.timestamp.isValid())
        return false;
    auto s = prepare(db_, "INSERT OR REPLACE INTO samples VALUES(?,?,?,?,?,?,?,?)");
    if (!s) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_int64(s.get(), 1, snapshot.timestamp.toMSecsSinceEpoch());
    bindOptional(s.get(), 2, snapshot.cpuPercent);
    sqlite3_bind_int64(s.get(), 3, snapshot.memoryValid ? snapshot.memoryUsed : 0);
    sqlite3_bind_int64(s.get(), 4, snapshot.memoryValid ? snapshot.memoryTotal : 0);
    bindOptional(s.get(), 5, snapshot.downloadBytesPerSec);
    bindOptional(s.get(), 6, snapshot.uploadBytesPerSec);
    const Volume* root = nullptr;
    for (const auto& v : snapshot.volumes) {
        if (v.mountPoint == "/System/Volumes/Data" || (!root && v.mountPoint == "/"))
            root = &v;
    }
    sqlite3_bind_int64(s.get(), 7,
                       root ? root->totalBytes - std::min(root->availableBytes, root->totalBytes) : 0);
    sqlite3_bind_int64(s.get(), 8, root ? root->totalBytes : 0);
    if (sqlite3_step(s.get()) != SQLITE_DONE) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

QVector<HistoryPoint> HistoryStore::query(qint64 sinceMs, int maxPoints) const {
    QVector<HistoryPoint> result;
    if (!db_ || maxPoints <= 0)
        return result;
    maxPoints = std::clamp(maxPoints, 1, 10000);
    // Average into time buckets in SQLite; large retention windows do not load all rows.
    auto bounds = prepare(db_, "SELECT MAX(timestamp_ms) FROM samples WHERE timestamp_ms>=?");
    if (!bounds) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return result;
    }
    sqlite3_bind_int64(bounds.get(), 1, sinceMs);
    if (sqlite3_step(bounds.get()) != SQLITE_ROW || sqlite3_column_type(bounds.get(), 0) == SQLITE_NULL)
        return result;
    const qint64 last = sqlite3_column_int64(bounds.get(), 0);
    const qint64 bucket = std::max<qint64>(1, (last - sinceMs) / maxPoints + 1);
    auto s =
        prepare(db_, "SELECT MAX(timestamp_ms),AVG(cpu),AVG(memory_used),AVG(memory_total),"
                     "AVG(download),AVG(upload),AVG(disk_used),AVG(disk_total) FROM samples "
                     "WHERE timestamp_ms>=? GROUP BY (timestamp_ms-?)/? ORDER BY MAX(timestamp_ms) LIMIT ?");
    if (!s) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return result;
    }
    sqlite3_bind_int64(s.get(), 1, sinceMs);
    sqlite3_bind_int64(s.get(), 2, sinceMs);
    sqlite3_bind_int64(s.get(), 3, bucket);
    sqlite3_bind_int(s.get(), 4, maxPoints);
    int rc;
    while ((rc = sqlite3_step(s.get())) == SQLITE_ROW) {
        HistoryPoint point;
        point.timestampMs = sqlite3_column_int64(s.get(), 0);
        point.bucketWidthMs = bucket;
        point.cpuPercent = optionalColumn(s.get(), 1);
        point.memoryUsed = static_cast<quint64>(sqlite3_column_int64(s.get(), 2));
        point.memoryTotal = static_cast<quint64>(sqlite3_column_int64(s.get(), 3));
        point.downloadBytesPerSec = optionalColumn(s.get(), 4);
        point.uploadBytesPerSec = optionalColumn(s.get(), 5);
        point.diskUsed = static_cast<quint64>(sqlite3_column_int64(s.get(), 6));
        point.diskTotal = static_cast<quint64>(sqlite3_column_int64(s.get(), 7));
        result.append(point);
    }
    if (rc != SQLITE_DONE)
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
    return result;
}
bool HistoryStore::prune(int days) {
    if (!db_)
        return false;
    auto s = prepare(db_, "DELETE FROM samples WHERE timestamp_ms < ?");
    if (!s) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_int64(s.get(), 1,
                       QDateTime::currentMSecsSinceEpoch() -
                           static_cast<qint64>(std::clamp(days, 1, 30)) * 86400000);
    if (sqlite3_step(s.get()) != SQLITE_DONE) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}
bool HistoryStore::clear() {
    if (!db_)
        return false;
    if (sqlite3_exec(db_, "DELETE FROM samples; PRAGMA wal_checkpoint(TRUNCATE); VACUUM;", nullptr, nullptr,
                     nullptr) != SQLITE_OK) {
        error_ = QString::fromUtf8(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}
} // namespace sentinel
