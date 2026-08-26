// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "nha_test_case.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <unistd.h>

/**
 * @brief Read cumulative CPU ticks (utime + stime) for a process from /proc/<pid>/stat.
 * @param pid  The process ID to query.
 * @return     Sum of user and system ticks, or -1 on failure.
 */
static long getCpuTicks(pid_t pid)
{
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    if (!stat.is_open()) return -1;

    std::string token;
    // Field 1: pid
    stat >> token;
    // Field 2: comm — enclosed in parens, may contain spaces
    char c;
    stat.get(c); // space
    stat.get(c); // '('
    while (stat.get(c) && c != ')') {}
    // Fields 3–13: state through cminflt
    for (int i = 3; i <= 13; ++i) stat >> token;
    // Field 14: utime, Field 15: stime
    long utime, stime;
    stat >> utime >> stime;
    return utime + stime;
}

/**
 * @brief Test validates that the NHA daemon consumes negligible CPU while idle.
 *        Sends a single query first to ensure the daemon is up, then measures
 *        CPU usage over a 1-second idle period via /proc/<pid>/stat.
 */
TEST_F(NHATestCase, Performance_idle_cpu_usage)
{
    // Send a header-only query so the daemon has fully handled at least one connection
    send_query("{H}");

    constexpr int intervalMs = 1000;
    constexpr double thresholdPercent = 1.0;

    const long ticksPerSec = sysconf(_SC_CLK_TCK);
    const long before = getCpuTicks(nha_process.pid);
    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    const long after = getCpuTicks(nha_process.pid);

    const double cpuPercent = static_cast<double>(after - before) / ticksPerSec / (intervalMs / 1000.0) * 100.0;

    EXPECT_LT(cpuPercent, thresholdPercent)
        << "NHA should consume < " << thresholdPercent << "% CPU while idle";
}
