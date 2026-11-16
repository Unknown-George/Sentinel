#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <cstdint>
#include <optional>

namespace sentinel {
struct Process {
    int pid = 0;
    QString name, user, path;
    std::optional<double> cpuPercent;
    quint64 residentBytes = 0;
    quint64 startTime = 0;
};
struct NetworkInterface {
    QString name, address;
    bool up = false, loopback = false;
    quint64 receivedBytes = 0, sentBytes = 0;
    std::optional<double> downloadBytesPerSec, uploadBytesPerSec;
};
struct Connection {
    int pid = 0;
    QString process, protocol, localAddress, remoteAddress, state;
};
struct Volume {
    QString name, mountPoint, filesystem;
    quint64 totalBytes = 0, availableBytes = 0;
    bool readOnly = false;
};
struct SecurityCheck {
    QString name, status, detail;
    // enabled, disabled, unknown, or info; no aggregate security score.
    QString level = "unknown";
};
struct SystemInfo {
    QString model, cpuModel, osVersion, architecture, hostname;
    int logicalCores = 0, physicalCores = 0;
    quint64 physicalMemory = 0;
    QString gpuName;
    quint64 gpuRecommendedWorkingSet = 0;
    bool gpuUnifiedMemory = false;
    bool reducedMotion = false;
};
struct Snapshot {
    QDateTime timestamp;
    std::optional<double> cpuPercent;
    QVector<double> corePercent;
    quint64 memoryTotal = 0, memoryUsed = 0, memoryAvailable = 0;
    quint64 memoryWired = 0, memoryCompressed = 0, memoryActive = 0;
    quint64 memoryInactive = 0, memoryFree = 0, memoryPurgeable = 0;
    quint64 swapUsed = 0, swapTotal = 0;
    bool swapValid = false;
    bool memoryValid = false;
    QString memoryPressure = "Unknown";
    std::optional<double> downloadBytesPerSec, uploadBytesPerSec;
    quint64 uptimeSeconds = 0;
    bool uptimeValid = false;
    QVector<Process> processes;
    QVector<NetworkInterface> interfaces;
    QVector<Volume> volumes;
    QStringList errors;
    QStringList notices;
};
struct HistoryPoint {
    qint64 timestampMs = 0;
    qint64 bucketWidthMs = 0; // Zero for a live observation; history reports its actual averaging window.
    std::optional<double> cpuPercent;
    quint64 memoryUsed = 0, memoryTotal = 0;
    std::optional<double> downloadBytesPerSec, uploadBytesPerSec;
    quint64 diskUsed = 0, diskTotal = 0;
};
} // namespace sentinel
Q_DECLARE_METATYPE(sentinel::Snapshot)
Q_DECLARE_METATYPE(sentinel::SystemInfo)
Q_DECLARE_METATYPE(QVector<sentinel::SecurityCheck>)
Q_DECLARE_METATYPE(QVector<sentinel::Connection>)
