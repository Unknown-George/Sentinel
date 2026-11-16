#include "MonitorService.h"
#include "HistoryStore.h"
#include "platform/Collector.h"
#include <QElapsedTimer>
#include <algorithm>
#include <memory>

namespace sentinel {
class MonitorWorker final : public QObject {
    Q_OBJECT
  public:
    explicit MonitorWorker(QString path) : path_(std::move(path)) {}
    void initialize(int interval, int days) {
        if (collector_)
            return;
        retentionDays_ = std::clamp(days, 1, 30);
        collector_ = std::make_unique<Collector>();
        store_ = std::make_unique<HistoryStore>(path_);
        if (!store_->isOpen())
            emit storageError(store_->error());
        else if (!store_->prune(retentionDays_))
            emit storageError(store_->error());
        emit systemInfoReady(collector_->systemInfo());
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &MonitorWorker::sample);
        timer_->setTimerType(Qt::CoarseTimer);
        timer_->start(std::clamp(interval, 1000, 10000));
        sample();
        history(300);
        // Security subprocesses run once off the UI thread and only on explicit refresh afterward.
        QTimer::singleShot(500, this, [this] { security(); });
    }
    void sample() {
        if (!collector_)
            return;
        const auto snapshot = collector_->collect();
        emit snapshotReady(snapshot);
        if (store_ && store_->isOpen() && (!persistClock_.isValid() || persistClock_.elapsed() >= 5000)) {
            if (!store_->append(snapshot))
                emit storageError(store_->error());
            persistClock_.restart();
            if (++persistCount_ % 720 == 0 && !store_->prune(retentionDays_))
                emit storageError(store_->error());
        }
        if (historyClock_.isValid() && historyClock_.elapsed() >= 60000)
            history(historySeconds_);
    }
    void interval(int ms) {
        if (timer_)
            timer_->setInterval(std::clamp(ms, 1000, 10000));
    }
    void retention(int days) {
        retentionDays_ = std::clamp(days, 1, 30);
        if (store_ && store_->isOpen() && !store_->prune(retentionDays_))
            emit storageError(store_->error());
    }
    void history(int seconds) {
        historySeconds_ = std::clamp(seconds, 60, 30 * 86400);
        if (store_) {
            emit historyReady(store_->query(QDateTime::currentMSecsSinceEpoch() -
                                            static_cast<qint64>(historySeconds_) * 1000));
            historyClock_.restart();
        }
    }
    void security() {
        if (collector_)
            emit securityReady(collector_->securityChecks());
    }
    void connections(int pid) {
        if (!collector_)
            return;
        QString error;
        const auto result = collector_->connections(pid, &error);
        emit connectionsReady(result, error);
    }
    void signing(const QString& path) {
        if (collector_)
            emit signingReady(collector_->signingInfo(path));
    }
    void clear() {
        if (store_ && store_->clear())
            emit historyCleared();
        else if (store_)
            emit storageError(store_->error());
    }
  signals:
    void snapshotReady(const sentinel::Snapshot& snapshot);
    void systemInfoReady(const sentinel::SystemInfo& info);
    void historyReady(const QVector<sentinel::HistoryPoint>& history);
    void securityReady(const QVector<sentinel::SecurityCheck>& checks);
    void connectionsReady(const QVector<sentinel::Connection>& connections, const QString& error);
    void signingReady(const sentinel::SecurityCheck& check);
    void storageError(const QString& error);
    void historyCleared();

  private:
    QString path_;
    std::unique_ptr<Collector> collector_;
    std::unique_ptr<HistoryStore> store_;
    QTimer* timer_ = nullptr;
    QElapsedTimer persistClock_;
    QElapsedTimer historyClock_;
    int historySeconds_ = 300;
    int retentionDays_ = 7, persistCount_ = 0;
};

MonitorService::MonitorService(const QString& databasePath, QObject* parent) : QObject(parent) {
    qRegisterMetaType<sentinel::Snapshot>();
    qRegisterMetaType<sentinel::SystemInfo>();
    qRegisterMetaType<QVector<sentinel::HistoryPoint>>();
    qRegisterMetaType<QVector<sentinel::SecurityCheck>>();
    qRegisterMetaType<QVector<sentinel::Connection>>();
    qRegisterMetaType<sentinel::SecurityCheck>();
    worker_ = new MonitorWorker(databasePath);
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &MonitorWorker::snapshotReady, this, &MonitorService::snapshotReady);
    connect(worker_, &MonitorWorker::systemInfoReady, this, &MonitorService::systemInfoReady);
    connect(worker_, &MonitorWorker::historyReady, this, &MonitorService::historyReady);
    connect(worker_, &MonitorWorker::securityReady, this, &MonitorService::securityReady);
    connect(worker_, &MonitorWorker::connectionsReady, this, &MonitorService::connectionsReady);
    connect(worker_, &MonitorWorker::signingReady, this, &MonitorService::signingReady);
    connect(worker_, &MonitorWorker::storageError, this, &MonitorService::storageError);
    connect(worker_, &MonitorWorker::historyCleared, this, &MonitorService::historyCleared);
    thread_.setObjectName("Sentinel collection");
    thread_.start();
}
MonitorService::~MonitorService() {
    thread_.quit();
    thread_.wait(); // all collector and database resources are destroyed on their owning thread.
}
void MonitorService::start(int intervalMs, int retentionDays) {
    QMetaObject::invokeMethod(
        worker_, [this, intervalMs, retentionDays] { worker_->initialize(intervalMs, retentionDays); });
}
void MonitorService::setInterval(int ms) {
    QMetaObject::invokeMethod(worker_, [this, ms] { worker_->interval(ms); });
}
void MonitorService::setRetention(int days) {
    QMetaObject::invokeMethod(worker_, [this, days] { worker_->retention(days); });
}
void MonitorService::requestHistory(int seconds) {
    QMetaObject::invokeMethod(worker_, [this, seconds] { worker_->history(seconds); });
}
void MonitorService::requestSecurity() {
    QMetaObject::invokeMethod(worker_, [this] { worker_->security(); });
}
void MonitorService::requestConnections(int pid) {
    QMetaObject::invokeMethod(worker_, [this, pid] { worker_->connections(pid); });
}
void MonitorService::requestSigning(const QString& path) {
    QMetaObject::invokeMethod(worker_, [this, path] { worker_->signing(path); });
}
void MonitorService::clearHistory() {
    QMetaObject::invokeMethod(worker_, [this] { worker_->clear(); });
}
} // namespace sentinel
#include "MonitorService.moc"
