// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef TPMWATCHER_H
#define TPMWATCHER_H

#include <condition_variable>
#include <mutex>
#include <stdint.h>
#include <thread>

#include "monotonictimer.h"

class TPMWatcher
{
public:
    /**
     * @brief Length of the device ID in bytes.
     */
    static const unsigned deviceIDLength = 16;

    /**
     * @brief Constructor
     * @param[in] intervalInMs   the interval with which the watcher thread should wake up
     *                           and update the cached device ID and last updated time
     */
    TPMWatcher(unsigned intervalInMs, const MonotonicTimer &monotonicTimer);

    TPMWatcher(const TPMWatcher &) = delete;
    void operator=(const TPMWatcher &) = delete;

    /**
     * @brief Gets the currently cached device ID.
     * @param[out] deviceID   buffer where the device ID shall be copied;
     *                        must be (at least) #deviceIDLength bytes long
     * @pre update() has been called, or else the result will be a series of zero bytes.
     */
    void get(uint8_t *deviceID);

    /**
     * @brief Gets the time in nanoseconds when the device ID was last updated.
     * @return   time in nanoseconds when the device ID was last updated
     * @pre update() has been called, or else the result will be zero.
     */
    uint64_t getLastUpdated();

    /**
     * @brief Starts the timer after updating (via TPM) the cached device ID once in the calling thread.
     * @return true if updating was successful and the thread was started, false in case of errors
     * @pre The timer is not running.
     * @post get() gives a valid device ID and getLastUpdated() gives a valid time.
     */
    bool start();

private:
    Timer timer;
    const unsigned intervalInMs;   // Timer should fire this often
                                   // Note: no need to protect with mutex, because it is constant
                                   // (more precisely, initialised by the main thread once,
                                   // and after that, only read by the timer.)
    const MonotonicTimer &monotonicTimer;

    uint8_t deviceID[deviceIDLength];      // Cache for the device ID, updated periodically by the timer
    std::mutex deviceIDLastUpdatedMutex;   // Mutex for deviceID and last updated time

    uint64_t lastUpdated;         // Timestamp in nanoseconds when the device ID was last updated

    /**
     * @brief Gets the device ID from the TPM and updates the cache variable with it. Also updates last updated time.
     * @return true if update was successful
     */
    bool update();
};

#endif
