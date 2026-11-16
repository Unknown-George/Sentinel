#pragma once
#include "core/Types.h"
#include <memory>

namespace sentinel {
// Construct and call on the dedicated collection thread only.
class Collector {
  public:
    Collector();
    ~Collector();
    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;
    SystemInfo systemInfo();
    Snapshot collect();
    QVector<SecurityCheck> securityChecks();
    QVector<Connection> connections(int pid, QString* error = nullptr);
    SecurityCheck signingInfo(const QString& executablePath);

  private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
} // namespace sentinel
