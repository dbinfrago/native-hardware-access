// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "monotonictimer.h"
#include "responder.h"
#include "tpmwatcher.h"
#include "configurationparser.h"

#include <iomanip>
#include <iostream>
#include <chrono>

#include <sched.h>
#include <cstring>
#include <systemd/sd-daemon.h>


bool pin_to_first_core()
{
    // Find out, which cores are allowed for this process
    cpu_set_t set;
    if (sched_getaffinity(0, sizeof(set), &set) != 0)
    {
        const auto error_code = errno;
        std::cerr << "FATAL: sched_getaffinity: " << strerror(error_code) << " - Failed to get CPU affinity" << std::endl;
        return false;
    }

    // Pick the first allowed core
    int core = -1;
    for (int i = 0; i < CPU_SETSIZE; i++)
    {
        if (CPU_ISSET(i, &set))
        {
            core = i;
            break;
        }
    }
    std::cout << "INFO: restricting process to single core: " << core << std::endl;

    // Restrict to that core
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
    {
        const auto error_code = errno;
        std::cerr << "FATAL: sched_setaffinity: " << strerror(error_code) << " - Failed to set CPU affinity" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "NativeHardwareAccess " << NHA_VERSION << std::endl;

    // We want to run on a single core only. This is to catch any
    // TSC inconsistencies due to unsynchronized TSCs on different cores.
    if (!pin_to_first_core())
    {
        sd_notify(0, "STATUS=Failed to pin to a single core!");
        return EXIT_FAILURE;
    }

    // Parse command line arguments
    std::string configFilename;
    if (argc < 2)
    {
        std::cerr << "FATAL: -c option is mandatory!" << std::endl;
        sd_notify(0, "STATUS=No configuration file provided!");
        return EXIT_FAILURE;
    }
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-c")
        {
            if (i + 1 < argc)
            {
                configFilename = argv[++i];
            }
            else
            {
                std::cerr << "FATAL: -c option requires a file argument!" << std::endl;
                sd_notify(0, "STATUS=No configuration file provided!");
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr << "INFO: Usage " << argv[0] << " -c <CONFIG_FILE>" << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Parse configuration file
    sd_notify(0, "STATUS=Parsing configuration...");
    ConfigurationParser config;
    if (!config.load(configFilename))
    {
        std::cerr << "FATAL: could not parse configuration file: " << configFilename << std::endl;
        sd_notify(0, "STATUS=Could not parse configuration file!");
        return EXIT_FAILURE;
    }

    for (const auto key : {"host", "port", "max_connections", "tpm_interval_in_ms", "tsc_check_interval_in_ms", "tsc_check_deviation_threshold"})
    {
        if (!config.hasKey(key))
        {
            std::cerr << "FATAL: missing mandatory configuration field: " << key << std::endl;
            const std::string message = "STATUS=Missing mandatory configuration field: " + std::string{key};
            sd_notify(0, message.c_str());
            return EXIT_FAILURE;
        }
    }

    int maxConnections;
    long tpmIntervalInMS;
    long long tscCheckIntervalInMs;
    double tscCheckDeviationThreshold;

    try
    {
        maxConnections = std::stoi(config.get("max_connections"));
        std::cout << "INFO: value of maxConnections is " << maxConnections << std::endl;
    }
    catch (...)
    {
        std::cerr << "FATAL: invalid value specified for max_connections." << std::endl;
        sd_notify(0, "STATUS=Invalid value specified for max_connections!");
        return EXIT_FAILURE;
    }

    try
    {
        tpmIntervalInMS = std::stol(config.get("tpm_interval_in_ms"));
        std::cout << "INFO: value of tpmIntervalInMS is " << tpmIntervalInMS << std::endl;
    }
    catch (...)
    {
        std::cerr << "FATAL: invalid value specified for tpm_interval_in_ms." << std::endl;
        sd_notify(0, "STATUS=Invalid value specified for tpm_interval_in_ms!");
        return EXIT_FAILURE;
    }

    try
    {
        tscCheckIntervalInMs = std::stoll(config.get("tsc_check_interval_in_ms"));
        std::cout << "INFO: value of tscCheckIntervalInMs is " << tscCheckIntervalInMs << std::endl;
    }
    catch (...)
    {
        std::cerr << "FATAL: invalid value specified for tsc_check_interval_in_ms." << std::endl;
        sd_notify(0, "STATUS=Invalid value specified for tsc_check_interval_in_ms!");
        return EXIT_FAILURE;
    }

    try
    {
        tscCheckDeviationThreshold = std::stod(config.get("tsc_check_deviation_threshold"));
        std::cout << "INFO: value of tscCheckDeviationThreshold is " << tscCheckDeviationThreshold << std::endl;
    }
    catch (...)
    {
        std::cerr << "FATAL: invalid value specified for tsc_check_deviation_threshold." << std::endl;
        sd_notify(0, "STATUS=Invalid value specified for tscCheckDeviationThreshold!");
        return EXIT_FAILURE;
    }

    // We now know the configuration, initialise components
    sd_notify(0, "STATUS=Initialising...");

    std::optional<MonotonicTimer> monotonicTimer;

    try
    {
        monotonicTimer.emplace(std::chrono::milliseconds(tscCheckIntervalInMs), tscCheckDeviationThreshold);
    }
    catch (...)
    {
        std::cerr << "FATAL: could not initialize MonotonicTimer." << std::endl;
        sd_notify(0, "STATUS=Could not initialize MonotonicTimer!");
        return EXIT_FAILURE;
    }

    if (!monotonicTimer)
    {
        std::cerr << "FATAL: could not construct MonotonicTimer." << std::endl;
        sd_notify(0, "STATUS=Could not construct MonotonicTimer!");
        return EXIT_FAILURE;
    }

    std::cout << "INFO: detected TSC frequency is " << monotonicTimer->getFrequency() << " MHz" << std::endl;

    TPMWatcher tpmWatcher(tpmIntervalInMS, *monotonicTimer);
    if (false == tpmWatcher.start())
    {
        std::cerr << "FATAL: could not start TPMWatcher." << std::endl;
        sd_notify(0, "STATUS=Could not start TPMWatcher!");
        return EXIT_FAILURE;
    }

    Responder responder(config.get("host"), config.get("port"), maxConnections, tpmWatcher, *monotonicTimer);
    if (false == responder.setupServer())
    {
        std::cerr << "FATAL: could not setup server." << std::endl;
        sd_notify(0, "STATUS=Could not setup server!");
        return EXIT_FAILURE;
    }
    std::cout << "INFO: Listening on " << config.get("host") << ":" << config.get("port") << std::endl;
    // Initialisation complete, start loop
    sd_notify(0, "READY=1\nSTATUS=Initialisation successful.");
    responder.start();

    return EXIT_SUCCESS;
}
