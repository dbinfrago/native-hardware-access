// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "responder.h"
#include "tpmwatcher.h"
#include "monotonictimer.h"

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

Responder::Responder(const std::string &ip, const std::string &port, const int maxConnections, TPMWatcher &tpmWatcher,  MonotonicTimer &monotonicTimer)
    : ip(ip),
      port(port),
      maxConnections(maxConnections),
      monotonicTimer(monotonicTimer),
      messageBuilder(tpmWatcher, monotonicTimer)
{
}

void Responder::start()
{
    while (monotonicTimer.getValid())
    {
        acceptNewClients();

        handleQueriesAndCleanUpConnections();

        waitForSocketActivity();
    }

    std::cerr << "ERROR: Time source is unreliable. Unable to process further requests." << std::endl;

    for (auto &socket: sockets)
    {
        close(socket.fd);
    }

    sockets = {};

    // We do not restart, since we do not want a new instance of this program to be started automatically.
    // The user should be informed about the problem and decide what to do next.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool Responder::setupServer()
{
    struct addrinfo addrinfoHints, *addrinfoList = nullptr;

    memset(&addrinfoHints, 0, sizeof(addrinfoHints));
    addrinfoHints.ai_family = AF_UNSPEC;
    addrinfoHints.ai_socktype = SOCK_STREAM;
    addrinfoHints.ai_flags = AI_PASSIVE;

    const int addrinfoResult = getaddrinfo(ip.c_str(), port.c_str(), &addrinfoHints, &addrinfoList);
    if (addrinfoResult != 0)
    {
        std::cerr << "FATAL: getaddrinfo(ip=" << ip << ", port=" << port << "): " 
                  << gai_strerror(addrinfoResult) << std::endl;
        return false;
    }

    if (nullptr == addrinfoList)
    {
        std::cerr << "FATAL: getaddrinfo(ip=" << ip << ", port=" << port << "): " 
                  << "no address information returned" << std::endl;
        return false;
    }

    if (nullptr != addrinfoList->ai_next)
    {
        std::cerr << "WARNING: getaddrinfo(ip=" << ip << ", port=" << port << "): " 
                  << "multiple results, the first one will be used" << std::endl;
    }

    int serverSocket = socket(addrinfoList->ai_family,
                              addrinfoList->ai_socktype,
                              addrinfoList->ai_protocol);

    if (-1 == serverSocket)
    {
        std::cerr << "FATAL: socket: " << strerror(errno) << std::endl;
        freeaddrinfo(addrinfoList);
        return false;
    }

    if (-1 == fcntl(serverSocket, F_SETFL, O_NONBLOCK))
    {
        std::cerr << "FATAL: fcntl: " << strerror(errno) << std::endl;
        close(serverSocket);
        freeaddrinfo(addrinfoList);
        return false;
    }

    const int yes = 1;
    if (-1 == setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        std::cerr << "ERROR: setsockopt: failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
    }

    if (-1 == bind(serverSocket, addrinfoList->ai_addr, addrinfoList->ai_addrlen))
    {
        std::cerr << "FATAL: bind: " << strerror(errno) << std::endl;
        close(serverSocket);
        freeaddrinfo(addrinfoList);
        return false;
    }

    if (-1 == listen(serverSocket, 128))
    {
        std::cerr << "FATAL: listen: " << strerror(errno) << std::endl;
        close(serverSocket);
        freeaddrinfo(addrinfoList);
        return false;
    }

    freeaddrinfo(addrinfoList);

    sockets.push_back({serverSocket, POLLIN, 0});

    return true;
}

void Responder::acceptNewClients()
{
    if (sockets.empty())
    {
        // defensive
        std::cerr << "FATAL: acceptNewClients: server socket missing! " << std::endl;
        exit(EXIT_FAILURE);
    }

    const int serverSocket = sockets[0].fd;
    while (true)
    {
        const int connectionSocket = accept(serverSocket, nullptr, nullptr);
        if (-1 == connectionSocket)
        {
            if ((EAGAIN == errno) || (EWOULDBLOCK == errno))
            {
                // No more connections to accept
                break;
            }
            else if (ECONNABORTED == errno)
            {
                // Connection error, but other connections may succeed, so repeat accept()
                std::cerr << "ERROR: accept: " << strerror(errno) << std::endl;
            }
            else
            {
                // Permanent error, no point repeating accept()
                std::cerr << "FATAL: accept: " << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (sockets.size() > maxConnections)
        {
            // Maximum number of connections reached, close accepted connection. Repeat accept for all remaining connection requests.
            std::cerr << "ERROR: maximum number of connections reached" << std::endl;
            close(connectionSocket);
        }
        else if (-1 == fcntl(connectionSocket, F_SETFL, O_NONBLOCK))
        {
            // New connection has been accepted, but failed to set non-blocking mode; do not use this connection
            std::cerr << "ERROR: fcntl: " << strerror(errno) << std::endl;
            close(connectionSocket);
        }
        else
        {
            // Successful client connection, add it to the list of sockets
            sockets.push_back({connectionSocket, POLLIN, 0});
        }
    }
}

void Responder::handleQueriesAndCleanUpConnections()
{
    if (sockets.empty())
    {
        // defensive
        std::cerr << "FATAL: handleQueriesAndCleanUpConnections: server socket missing! " << std::endl;
        exit(EXIT_FAILURE);
    }

    auto socketIt = sockets.begin();
    ++socketIt;  // skip first socket: the server (listening) socket
    while (monotonicTimer.getValid() && socketIt != sockets.end())
    {
        const int connectionSocket = socketIt->fd;

        // Try to receive a query
        int recvSize = 0;
        // We have non-blocking sockets, so we may need multiple recv() calls to get all data.
        // If no data is available, recv() returns -1 with EAGAIN or EWOULDBLOCK.
        int recvResult = recv(connectionSocket, recvBuffer, maxSizeOfMessage, 0);
        // Iterate, if we received data (recvResult > 0) and we have not yet filled our buffer completely (recvSize < maxSizeOfMessage)
        while ((recvResult > 0) && (recvSize < maxSizeOfMessage))
        {
            recvSize += recvResult;
            recvResult = recv(connectionSocket, recvBuffer + recvSize, maxSizeOfMessage - recvSize, 0);
        }

        // recv() returning 0 means connection closed by peer.
        if (0 == recvResult)
        {
            // Connection closed by client.
            // Close and remove this connection from the list:
            close(connectionSocket);
            socketIt = sockets.erase(socketIt);
        }
        // recv() returning -1 with EAGAIN or EWOULDBLOCK means no more data available right now.
        // So anything else in errno is an error.
        else if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
        {
            // "Real" error occurred while receiving
            std::cerr << "ERROR: recv: " << strerror(errno) << std::endl;

            // Close and remove this connection from the list:
            close(connectionSocket);
            socketIt = sockets.erase(socketIt);
        }
        else if (0 == recvSize)
        {
            // No data was received on this connection. Next:
            ++socketIt;
        }
        else
        {
            // Successful reception of query, try to create response
            const int sendSize = messageBuilder.createResponse(recvBuffer, recvSize, sendBuffer, maxSizeOfMessage);
            if (sendSize == -1)
            {
                // Error probably due to bad query
                std::cerr << "ERROR: failed to create response" << std::endl;

                // Close and remove this connection from the list:
                close(connectionSocket);
                socketIt = sockets.erase(socketIt);
            }
            else
            {
                // Response successfully assembled, try to send
                const int sendResult = send(connectionSocket, sendBuffer, sendSize, MSG_NOSIGNAL);
                if (sendResult == sendSize)
                {
                    // Successful sending of response
                    // This connection has been served. Next:
                    ++socketIt;
                }
                else
                {
                    // Failed to send the response
                    if (-1 == sendResult)
                    {
                        // Some error
                        std::cerr << "ERROR: send: " << strerror(errno) << std::endl;
                    }
                    else
                    {
                        // Partial send, also unacceptable
                        std::cerr << "ERROR: send: failed to send the whole response. Response size is "
                                  << sendSize << " bytes, but only "  << sendResult << " bytes sent" << std::endl;
                    }

                    // Close and remove this connection from the list:
                    close(connectionSocket);
                    socketIt = sockets.erase(socketIt);
                }
            }
        }
    }
}

void Responder::waitForSocketActivity()
{
    // Make sure we yield CPU while waiting for socket activity
    if (-1 == poll(sockets.data(), sockets.size(), -1))
    {
        std::cerr << "FATAL: poll: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
}
