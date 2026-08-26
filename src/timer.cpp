// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "timer.h"

Timer::Timer()
    : running(true)
{
}

Timer::~Timer()
{
    stop();
}

void Timer::start(std::function<void()> callback, std::chrono::milliseconds interval)
{
    thread = std::thread([this, callback, interval]()
    {
        std::unique_lock<std::mutex> uniqueLock(mutex);

        while (true)
        {
            auto stopped = [this]
            {
                return !running;
            };

            if (conditionVariable.wait_for(uniqueLock, interval, stopped))
            {
                break;
            }

            uniqueLock.unlock();

            try
            {
                if (callback)
                {
                    callback();
                }
            }
            catch (...)
            {
            }

            uniqueLock.lock();
        }
    });
}

void Timer::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);

        running = false;
    }

    conditionVariable.notify_one();

    if (thread.joinable())
    {
        thread.join();
    }
}
