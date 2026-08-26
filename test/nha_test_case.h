// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef NHA_TEST_CASE_H
#define NHA_TEST_CASE_H

#include "gtest/gtest.h"

#include <string>
#include <unordered_map>

// defined by CMake
#ifndef NHA_BIN_PATH
#define NHA_BIN_PATH "./nha"
#endif

#ifndef NHAC_BIN_PATH
#define NHAC_BIN_PATH "./nhac"
#endif

#ifndef NHA_CONF_PATH
#define NHA_CONF_PATH "./nha.conf"
#endif

#ifndef NHA_IP
#define NHA_IP "127.0.0.1"
#endif

#ifndef NHA_PORT
#define NHA_PORT "7872"
#endif

struct Process
{
    pid_t pid = -1;
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
};

/**
 * @brief Common Test Case class for GTest cases that will use the NHA and the NHAC test tool.
 * Manages the lifecycle of the NHA daemon process.
 * Provides a synchronous helper function to execute the NHAC client.
 */
class NHATestCase : public ::testing::Test
{
protected:
    Process nha_process;
    /**
     * @brief Starts the nha daemon process.
     * @throws std::runtime_error if the fork/exec or pipe creation fails.
     */
    void SetUp() override;

    /**
     * @brief Terminates the nha daemon process.
     */
    void TearDown() override;

    /**
     * @brief Retreive the STDOUT and STDERR content of the daemon process.
     * @return A pair<string, string> containing the captured stdout and stderr streams, respectively.
     */
    std::pair<std::string, std::string> get_daemon_output();

    /**
     * @brief Execture a query toward the NHA using the NHAC
     * @param query The string (the query) to feed into nhac's stdin.
     * @param expected_exit_code The expected exit code of the NHAC, default 0 (success).
     * @return a hash map of the parsed fields - TLV records have both a "tag-value" (both hexstring)
     *         pair and a "tag_decoded-decoded string" key-value pair.
     *         additional mandatory keys are: completeSizeOfMsgInBytes, received, calculated and xored
     */
    std::unordered_map<std::string, std::string> send_query(const std::string &query, const int expected_exit_code = 0);

private:
    /**
     * @brief Synchronously executes the NHAC
     * @param ip The IP address where the NHA is listening.
     * @param port The port where the NHA is listening.
     * @param data The string (the query) to feed into nhac's stdin.
     * @param exit_code Reference to an integer that will receive nhac's exit code.
     * @return A pair<string, string> containing the captured stdout and stderr streams, respectively.
     * @throws std::runtime_error if the process execution fails.
     */
    std::pair<std::string, std::string> call_nhac(const char *const ip, const char *const port, const std::string &data, int &exit_code);

    /**
     * @brief Parses a response from the NHA described by the NHAC output format.
     * @param message the output of the NHAC that describes a response
     * @return a hash map of the parsed fields - TLV records have both a "tag-value" (both hexstring)
     *         pair and a "tag_decoded-decoded string" key-value pair.
     *         additional mandatory keys are: completeSizeOfMsgInBytes, received, calculated and xored
     */
    static std::unordered_map<std::string, std::string> parse_nhac_output(const std::string &message);
};

#endif // NHA_TEST_CASE_H
