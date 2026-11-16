#include "core/MetricsMath.h"
#include <gtest/gtest.h>
#include <limits>

namespace sentinel {
TEST(CpuUsage, UsesDifferencesAndAllFourStates) {
    const CpuTicks previous{1000, 400, 3000, 100};
    const CpuTicks current{1030, 420, 3040, 110};
    const auto value = cpuUsage(previous, current);
    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(*value, 60.0);
}
TEST(CpuUsage, IdleAndBusyBoundaries) {
    EXPECT_DOUBLE_EQ(*cpuUsage({1, 1, 1, 1}, {1, 1, 11, 1}), 0.0);
    EXPECT_DOUBLE_EQ(*cpuUsage({1, 1, 1, 1}, {11, 1, 1, 1}), 100.0);
}
TEST(CpuUsage, RejectsNoIntervalAndResetCounters) {
    EXPECT_FALSE(cpuUsage({1, 2, 3, 4}, {1, 2, 3, 4}));
    EXPECT_FALSE(cpuUsage({10, 20, 30, 40}, {9, 21, 31, 41}));
    EXPECT_FALSE(cpuUsage({10, 20, 30, 40}, {11, 19, 31, 41}));
    EXPECT_FALSE(cpuUsage({10, 20, 30, 40}, {11, 21, 29, 41}));
    EXPECT_FALSE(cpuUsage({10, 20, 30, 40}, {11, 21, 31, 39}));
}
TEST(CounterRate, UsesElapsedMonotonicTime) {
    ASSERT_TRUE(counterRate(100, 1124, 0.5));
    EXPECT_DOUBLE_EQ(*counterRate(100, 1124, 0.5), 2048.0);
    EXPECT_DOUBLE_EQ(*counterRate(100, 100, 2.0), 0.0);
}
TEST(CounterRate, RejectsCounterResetAndInvalidElapsedTime) {
    EXPECT_FALSE(counterRate(100, 99, 1.0));
    EXPECT_FALSE(counterRate(0, 100, 0.0));
    EXPECT_FALSE(counterRate(0, 100, -1.0));
    EXPECT_FALSE(counterRate(0, 100, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(counterRate(0, 100, std::numeric_limits<double>::infinity()));
}
TEST(ProcessCpuUsage, ACoreIsOneHundredPercentAndCanExceedIt) {
    ASSERT_TRUE(processCpuUsage(100, 2000000100, 1.0));
    EXPECT_DOUBLE_EQ(*processCpuUsage(100, 2000000100, 1.0), 200.0);
    EXPECT_FALSE(processCpuUsage(100, 99, 1.0));
    EXPECT_FALSE(processCpuUsage(100, 200, 0.0));
}
} // namespace sentinel
