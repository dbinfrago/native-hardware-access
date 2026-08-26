// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef MONOTONICTIMER_H
#define MONOTONICTIMER_H

#include "timer.h"

#include <atomic>
#include <optional>

class MonotonicTimer final
{
public:
    /**
     * @brief  Constructor  Check if appropriate TSC is available and reads TSC frequency from perf API.
     * @param[in]  interval   Interval to periodic calculation of TSC frequency.
     * @param[in]  deviation  The maximum deviation between read and measured frequency.
     */
    MonotonicTimer(std::chrono::milliseconds interval, double deviationThreshold);
    /**
     * @brief  Get elapsed time since boot in nanoseconds.
     * @return  Elapsed time since boot in nanoseconds.
     */
    uint64_t get() const;
    /**
     * @brief  Get TSC frequency in MHz.
     * @return  TSC frequency in MHz.
     */
    double getFrequency() const;
    /**
     * @brief  Valid flag signals if the difference of measured and obtained frequency are within limits.
     * @return  Value of valid flag.
     */
    bool getValid() const;

private:
    std::atomic<bool> valid;
    double deviationThreshold;
    double frequency;
    static constexpr uint64_t shift = 32;
    uint64_t nsPerTickMultiplier;
    std::chrono::milliseconds interval;
    Timer timer;
    uint64_t startTime;
    uint64_t startTsc;

    bool checkTsc() const;
    double readFrequency() const;
    void callback();
    uint64_t getTime() const;
    uint64_t getTsc() const;
};

#endif
