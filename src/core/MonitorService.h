#pragma once
#include "core/Types.h"
#include <QObject>
#include <QThread>
#include <QTimer>

namespace sentinel {
class MonitorWorker;
class MonitorService : public QObject {
    Q_OBJECT
  public:
    explicit MonitorService(const QString& databasePath, QObject* parent = nullptr);
    ~MonitorService() override;
    void start(int intervalMs, int retentionDays);
    void setInterval(int ms);
    void setRetention(int days);
    void requestHistory(int seconds);
    void requestSecurity();
    void requestConnections(int pid);
    void requestSigning(const QString& path);
    void clearHistory();
  signals:
    void snapshotReady(const sentinel::Snapshot& snapshot);
    void systemInfoReady(const sentinel::SystemInfo& info);
    void historyReady(const QVector<sentinel::HistoryPoint>& points);
    void securityReady(const QVector<sentinel::SecurityCheck>& checks);
    void connectionsReady(const QVector<sentinel::Connection>& connections, const QString& error);
    void signingReady(const sentinel::SecurityCheck& check);
    void storageError(const QString& error);
    void historyCleared();

  private:
    QThread thread_;
    MonitorWorker* worker_ = nullptr;
};
} // namespace sentinel
Q_DECLARE_METATYPE(QVector<sentinel::HistoryPoint>)
Q_DECLARE_METATYPE(sentinel::SecurityCheck)
