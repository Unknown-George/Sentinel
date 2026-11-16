#pragma once
#include "core/Types.h"
#include <QString>
struct sqlite3;

namespace sentinel {
class HistoryStore {
  public:
    explicit HistoryStore(const QString& path);
    ~HistoryStore();
    HistoryStore(const HistoryStore&) = delete;
    HistoryStore& operator=(const HistoryStore&) = delete;
    bool append(const Snapshot& snapshot);
    QVector<HistoryPoint> query(qint64 sinceMs, int maxPoints = 600) const;
    bool prune(int days);
    bool clear();
    bool isOpen() const { return db_ != nullptr; }
    QString error() const { return error_; }

  private:
    sqlite3* db_ = nullptr;
    mutable QString error_;
};
} // namespace sentinel
