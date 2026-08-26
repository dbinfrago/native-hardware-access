// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef RESPONDER_H
#define RESPONDER_H

#include <poll.h>

#include <vector>
#include <string>

#include "messagebuilder.h"

class Responder
{
public:
    /**
     * @brief Constructor
     * @param[in]  ip               IP address the server should listen on
     * @param[in]  port             TCP port the server should listen on
     * @param[in]  tpmWatcher       Reference to TPMWatcher instance that is used for getting device ID.
     *                              Must outlive this object!
     * @param[in]  monotonicTimer   Reference to MonotonicTimer instance that is used for getting elapsed time.
     *                              Must outlive this object!
     */
    Responder(const std::string &ip, const std::string &port, const int maxConnections, TPMWatcher &tpmWatcher,  MonotonicTimer &monotonicTimer);

    /**
     * @brief Initialise and start the server. Must be called before `start`!
     * @return true on success, false on failure
     */
    bool setupServer();

    /**
     * @brief Enter the main loop of serving client connections. Call `setupServer` before starting the main loop!
     */
    [[noreturn]] void start();

    static const int maxSizeOfMessage = 61440;

private:
    std::string ip;
    std::string port;
    int maxConnections;

    // List of active sockets; index 0 is the server (listening) socket, the rest are client connections
    std::vector<pollfd> sockets;

    // Static sized buffers for performance
    char recvBuffer[maxSizeOfMessage];
    char sendBuffer[maxSizeOfMessage];

    MonotonicTimer &monotonicTimer;
    MessageBuilder messageBuilder;

    void handleQueriesAndCleanUpConnections();
    void waitForSocketActivity();
    void acceptNewClients();
};

#endif
