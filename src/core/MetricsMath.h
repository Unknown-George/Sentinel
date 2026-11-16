#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace sentinel {
struct CpuTicks {
    std::uint64_t user = 0, system = 0, idle = 0, nice = 0;
};

// Counter resets/wraps and absent baselines are gaps, never activity spikes.
inline std::optional<double> cpuUsage(const CpuTicks& previous, const CpuTicks& current) {
    if (current.user < previous.user || current.system < previous.system || current.idle < previous.idle ||
        current.nice < previous.nice)
        return std::nullopt;
    const long double busy = static_cast<long double>(current.user - previous.user) +
                             (current.system - previous.system) + (current.nice - previous.nice);
    const long double total = busy + (current.idle - previous.idle);
    if (total <= 0)
        return std::nullopt;
    return std::clamp(static_cast<double>(100.0L * busy / total), 0.0, 100.0);
}

inline std::optional<double> counterRate(std::uint64_t previous, std::uint64_t current,
                                         double elapsedSeconds) {
    if (current < previous || !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0)
        return std::nullopt;
    const double value = static_cast<double>(current - previous) / elapsedSeconds;
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

// Process CPU is in core equivalents: 100% means one fully occupied core.
inline std::optional<double> processCpuUsage(std::uint64_t previousNs, std::uint64_t currentNs,
                                             double elapsedSeconds) {
    const auto nsPerSec = counterRate(previousNs, currentNs, elapsedSeconds);
    return nsPerSec ? std::optional<double>(*nsPerSec / 1.0e7) : std::nullopt;
}
} // namespace sentinel
