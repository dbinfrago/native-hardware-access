// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "monotonictimer.h"
#include "timer.h"

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <cpuid.h>
#include <unistd.h>
#include <x86intrin.h>
#include <iostream>

MonotonicTimer::MonotonicTimer(std::chrono::milliseconds interval, double deviationThreshold)
    : interval(interval), deviationThreshold(deviationThreshold), valid(true)
{
    if (!checkTsc())
    {
        throw std::exception();
    }

    frequency = readFrequency();

    if (frequency > 0.0)
    {
        // calculate a ratio and shift it high to use for fixed point arithmetic
        nsPerTickMultiplier = (static_cast<uint64_t>(1000) << shift) / frequency;
        startTime = getTime();
        startTsc = getTsc();

        timer.start(std::bind(&MonotonicTimer::callback, this), interval);
    }
    else
    {
        std::cerr << "ERROR: Cannot read the TSC frequency" << std::endl;
        throw std::exception();
    }
}

uint64_t MonotonicTimer::get() const
{
    constexpr uint64_t NANOSECONDS_PER_MICROSECOND = 1'000ULL;
    // subtract initial TSC value to defend against abnormal such values
    // then, cast this into a 128 bit type and multiply it for fixed point arithmetic, finally shifting back
    return static_cast<uint64_t>((static_cast<unsigned __int128>(getTsc() - startTsc) * nsPerTickMultiplier) >> shift);
}

double MonotonicTimer::getFrequency() const
{
    return frequency;
}

bool MonotonicTimer::getValid() const
{
    return valid;
}

bool MonotonicTimer::checkTsc() const
{
    constexpr unsigned int PROCESSOR_POWER_MANAGEMENT_INFORMATION = 0x80000007;
    constexpr unsigned int EXTENDED_PROCESSOR_INFORMATION = 0x80000001;

    constexpr unsigned int RDTSCP = 27;
    constexpr unsigned int TSC_INVARIANT = 8;

    if (__get_cpuid_max(0x80000000, nullptr) < PROCESSOR_POWER_MANAGEMENT_INFORMATION)
    {
        return false;
    }

    unsigned int eax, ebx, ecx, edx;

    if (!__get_cpuid(EXTENDED_PROCESSOR_INFORMATION, &eax, &ebx, &ecx, &edx) ||
        !(edx & (1ULL << RDTSCP)))
    {
        return false;
    }

    if (!__get_cpuid_count(PROCESSOR_POWER_MANAGEMENT_INFORMATION, 0, &eax, &ebx, &ecx, &edx) ||
        !(edx & (1ULL << TSC_INVARIANT)))
    {
        return false;
    }

    return true;
}

double MonotonicTimer::readFrequency() const
{
    constexpr pid_t processId = 0;
    constexpr int cpu = -1;
    constexpr int groupFileDescriptor = -1;
    constexpr unsigned long flags = 0;

    constexpr double KILOHERTZ_PER_MEGAHERTZ = 1e3;

    perf_event_attr perfEventAttr{};

    perfEventAttr.type = PERF_TYPE_SOFTWARE;
    perfEventAttr.size = sizeof(perfEventAttr);
    perfEventAttr.config = PERF_COUNT_SW_CPU_CLOCK;
    perfEventAttr.disabled = 1;

    int fileDescriptor = syscall(__NR_perf_event_open, &perfEventAttr, processId, cpu, groupFileDescriptor, flags);

    if (fileDescriptor == -1)
    {
        return 0.0;
    }

    long pageSize = sysconf(_SC_PAGESIZE);

    if (pageSize == -1)
    {
        close(fileDescriptor);

        return 0.0;
    }

    void *page = mmap(nullptr, static_cast<size_t>(pageSize), PROT_READ, MAP_SHARED, fileDescriptor, 0);

    if (page == MAP_FAILED)
    {
        close(fileDescriptor);

        return 0.0;
    }

    auto *perfEventMmapPage = reinterpret_cast<perf_event_mmap_page *>(page);

    uint16_t shift = perfEventMmapPage->time_shift;
    double multiplier  = static_cast<double>(perfEventMmapPage->time_mult);

    munmap(page, pageSize);

    close(fileDescriptor);

    if (multiplier > 0.0)
    {
        return KILOHERTZ_PER_MEGAHERTZ * (1ULL << shift) / multiplier;
    }
    else
    {
        // A zero or negative multiplier is not a credible return value.
        // We may have failed to read the frequency, so we return 0.0 to indicate this.
        return 0.0;
    }
}

void MonotonicTimer::callback()
{
    constexpr uint64_t NANOSECONDS_PER_MICROSECOND = 1'000ULL;

    uint64_t time = getTime();
    uint64_t tsc = getTsc();

    if (time == startTime)
    {
        // We do not have a time delta, so we cannot calculate frequency.
        // To be careful, we declare the frequency as invalid while this persists.
        valid = false;
        return;
    }

    double frequency = NANOSECONDS_PER_MICROSECOND * static_cast<double>(tsc - startTsc) / static_cast<double>(time - startTime);

    double deviation = abs(this->frequency - frequency) / this->frequency;

    if (deviation >= deviationThreshold)
    {
        valid = false;
    }
}

uint64_t MonotonicTimer::getTime() const
{
    constexpr uint64_t NANOSECONDS_PER_SECOND = 1'000'000'000ULL;

    timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return static_cast<uint64_t>(ts.tv_sec) * NANOSECONDS_PER_SECOND + static_cast<uint64_t>(ts.tv_nsec);
}

uint64_t MonotonicTimer::getTsc() const
{
    [[maybe_unused]] unsigned int aux;

    return __rdtscp(&aux);
}
