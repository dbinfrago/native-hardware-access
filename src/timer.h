// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef TIMER_H
#define TIMER_H

#include <condition_variable>
#include <functional>
#include <thread>

/**
 * @brief  Implements a Timer class that calls a function perodically set by interval.
 */
class Timer final
{
public:
    /**
     * @brief  Constructor
     */
    Timer();
    /**
     * @brief  Destructor  Stops the timer before quitting
     */
    ~Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /**
     * @brief  Starts the timer
     * @param[in]  callback  Function to call when timer fires
     * @param[in]  interval  Time that elapses between two timer events
     */
    void start(std::function<void()> callback, std::chrono::milliseconds interval);
     /**
     * @brief  Stops the timer
     */
    void stop();

private:
    bool running;
    std::condition_variable conditionVariable;
    std::mutex mutex;
    std::thread thread;
};

#endif
