// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "nha_test_case.h"

#include <thread>
#include <chrono>
#include <sstream>

static void safe_close(int &fd)
{
    if (fd != -1)
    {
        close(fd);
        fd = -1;
    }
}

static std::string dump_pipe(int fd)
{
    std::string content;
    constexpr size_t buffer_size = 4096;
    std::vector<char> buffer(buffer_size);
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer.data(), buffer_size)) > 0)
    {
        content.append(buffer.data(), bytes_read);
    }
    return content;
}

static void setUpProcess(Process &p)
{
    if (pipe(p.stdin_pipe) == -1 ||
        pipe(p.stdout_pipe) == -1 ||
        pipe(p.stderr_pipe) == -1)
    {
        throw std::runtime_error("Failed to create pipes!");
    }
    p.pid = fork();
    if (p.pid == -1)
    {
        throw std::runtime_error("Failed to fork process!");
    }
    else if (p.pid == 0)
    {
        // child
        // close parent's side of the pipes
        safe_close(p.stdin_pipe[1]);
        safe_close(p.stdout_pipe[0]);
        safe_close(p.stderr_pipe[0]);
        // redirect
        dup2(p.stdin_pipe[0], STDIN_FILENO);
        dup2(p.stdout_pipe[1], STDOUT_FILENO);
        dup2(p.stderr_pipe[1], STDERR_FILENO);
        // close redundant pipes after dup2
        safe_close(p.stdin_pipe[0]);
        safe_close(p.stdout_pipe[1]);
        safe_close(p.stderr_pipe[1]);
    }
    else
    {
        // parent
        safe_close(p.stdin_pipe[0]);
        safe_close(p.stdout_pipe[1]);
        safe_close(p.stderr_pipe[1]);
    }
}

void NHATestCase::SetUp()
{
    setUpProcess(nha_process);
    // child
    if (nha_process.pid == 0)
    {
        // launch NHA daemon
        char *const argv[] = {
            (char *)NHA_BIN_PATH,
            (char *)"-c",
            (char *)NHA_CONF_PATH,
            nullptr};
        execv(argv[0], argv);
        _exit(1);
    }
    // parent
    else
    {
        // allow for nha to start up
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        int status;
        if (waitpid(nha_process.pid, &status, WNOHANG) == nha_process.pid)
        {
            throw std::runtime_error("nha exited immediately upon launch. Check path/permissions.");
        }
    }
}

void NHATestCase::TearDown()
{
    if (nha_process.pid > 0)
    {
        kill(nha_process.pid, SIGTERM);
    }
    safe_close(nha_process.stdin_pipe[1]);
    safe_close(nha_process.stdout_pipe[0]);
    safe_close(nha_process.stderr_pipe[0]);
    nha_process.pid = -1;
}

std::pair<std::string, std::string> NHATestCase::call_nhac(
    const char *const ip,
    const char *const port,
    const std::string &data,
    int &exit_code)
{
    Process client;
    exit_code = -1;
    setUpProcess(client);
    // child
    if (client.pid == 0)
    {
        char *const argv[] = {
            (char *)NHAC_BIN_PATH,
            (char *)"-q",
            (char *)ip,
            (char *)port,
            nullptr};
        execv(argv[0], argv);
        _exit(1);
    }
    // parent
    else
    {
        // write to nhac's stdin, then close
        write(client.stdin_pipe[1], data.data(), data.size());
        safe_close(client.stdin_pipe[1]);

        int status;
        waitpid(client.pid, &status, 0);

        std::string stdout_output = dump_pipe(client.stdout_pipe[0]);
        std::string stderr_output = dump_pipe(client.stderr_pipe[0]);
        safe_close(client.stdout_pipe[0]);
        safe_close(client.stderr_pipe[0]);

        // output
        if (WIFEXITED(status))
        {
            exit_code = WEXITSTATUS(status);
        }
        return {stdout_output, stderr_output};
    }
}

std::pair<std::string, std::string> NHATestCase::get_daemon_output()
{
    return std::make_pair<std::string, std::string>(dump_pipe(nha_process.stdout_pipe[0]), dump_pipe(nha_process.stderr_pipe[0]));
}

std::unordered_map<std::string, std::string> NHATestCase::parse_nhac_output(const std::string &message)
{
    std::unordered_map<std::string, std::string> res;
    std::stringstream ss(message);
    std::string line;
    while (std::getline(ss, line))
    {
        if (line.empty())
            continue;
        std::stringstream line_ss(line);
        std::string token;
        while (line_ss >> token)
        {
            if (token.substr(0, 25) == "completeSizeOfMsgInBytes=")
            {
                res.emplace("completeSizeOfMsgInBytes", token.substr(25));
            }
            else if (token.substr(0, 2) == "T=")
            {
                // TLV begins with Tag
                const std::string tag = token.substr(2);
                std::string length_string, value_string, remaining_value, decoded;
                // next token is Length
                line_ss >> length_string;
                const int length = std::atoi(length_string.substr(2).c_str());
                // first token in Value has prefix
                line_ss >> value_string;
                value_string = value_string.substr(2);
                // then read rest of value until EOL
                std::getline(line_ss, remaining_value);
                value_string = value_string.append(remaining_value);
                res.emplace(tag, value_string);
                // get next line for decoded value
                std::getline(ss, decoded);
                // trim whitespace
                size_t start = decoded.find_first_not_of(" \n\r");
                if (start != std::string::npos)
                    decoded.erase(0, start);
                res.emplace(tag + "_decoded", decoded);
            }
            else if (token == "received")
            {
                std::string recMD5;
                line_ss >> recMD5;
                recMD5 = recMD5.substr(4);
                res.emplace("received", recMD5);
            }
            else if (token == "xored")
            {
                std::string xorMD5;
                line_ss >> xorMD5;
                xorMD5 = xorMD5.substr(4);
                res.emplace("xored", xorMD5);
            }
            else if (token == "calculated")
            {
                std::string calcMD5;
                line_ss >> calcMD5;
                calcMD5 = calcMD5.substr(4);
                res.emplace("calculated", calcMD5);
            }
        }
    }
    return res;
}

std::unordered_map<std::string, std::string> NHATestCase::send_query(const std::string &query, const int expected_exit_code)
{
    // call nha, get stdout and stderr
    int exit_code = -1;
    std::pair<std::string, std::string> res = call_nhac(NHA_IP, NHA_PORT, query, exit_code);
    EXPECT_EQ(expected_exit_code, exit_code)
        << "nhac client should exit with code 0 on a valid request.\n"
        << "nhac stderr was:\n"
        << res.second;
    // check stderr
    EXPECT_TRUE(res.second.empty() || expected_exit_code != 0)
        << "nhac client reported errors during execution.";

    return parse_nhac_output(res.first);
}
