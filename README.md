<!--
SPDX-FileCopyrightText: Copyright DB InfraGO AG
SPDX-License-Identifier: Apache-2.0
-->

# Native Hardware Access

## Overview

Native Hardware Access (NHA) is a Linux system service to support the safe execution of safety critical software components in a virtualized environment. It provides software running on the same physical host with access to hardware-backed identity and monotonic time information. It is intended to be run in parallel with virtualized safety-related applications requiring diverse provision of this very information.

NHA listens for client requests over TCP and responds using the NHA TLV protocol. It derives a Device ID from the host's physical Trusted Platform Module (TPM) and provides timestamps from a strictly monotonic timer based on the CPU Time Stamp Counter (TSC).


## Main Features

- Provides query-response access to hardware-derived information.
- Derives a Device ID from the physical TPM.
- Provides monotonic timing based on invariant TSC/RDTSCP.
- Runs as a systemd service.
- Is distributed as a Debian package.

## Runtime Requirements

- Linux host with a physical TPM.
- CPU support for invariant TSC and RDTSCP.
- Access to Linux perf API for TSC frequency measurement.
- `tss2-esys` / TPM2 Software Stack libraries.
- OpenSSL.
- systemd.

## Documentation

- Design specification: [doc/NHA_DS.md](doc/NHA_DS.md)
- Interface/protocol specification: [doc/NHA_IF.md](doc/NHA_IF.md)

---

## Building the Debian Package Locally

The NHA is distributed as a Debian package (`.deb` file). For details, see: [Design Specification](doc/NHA_DS.md).

The documented build and installation flow assumes a Debian-based Linux distribution with `apt`, `dpkg-buildpackage`, and systemd available.

Install the required build dependencies on the build host first:

```bash
sudo apt install debhelper-compat cmake pkg-config build-essential libssl-dev libsystemd-dev libtss2-dev
```

The packaging helper downloads the required `picohash.h` header during the build. The build host therefore needs network access, unless `PICOHASH_URL` is set to an accessible mirror.

To build the package, use the CI packaging helper script:

```bash
./ci/package.sh
```

The `nha_<VERSION>_<ARCH>.deb` file will be written to `deb_output/` on a successful build.

Find further details in the corresponding section of [CONTRIBUTING.md](CONTRIBUTING.md#prerequisites).

## Installing and Starting the Debian Package

Install the generated Debian package on the target host:

```bash
sudo apt install ./deb_output/nha_<VERSION>_<ARCH>.deb
```

Runtime package dependencies are installed automatically by `apt`. The package installs:

- `/usr/sbin/nha`
- `/usr/lib/systemd/system/nha.service`
- `/usr/share/nha/nha.conf.default`
- `/etc/sysctl.d/perf_event_paranoid.conf`
- `/etc/nha.conf`, created from the default configuration during package configuration

Review the configuration before starting the service, especially the listening address and port:

```bash
sudo editor /etc/nha.conf
```

The default configuration listens on localhost port `7872`:

```text
host 127.0.0.1
port 7872
```

For the full default configuration, see [resources/nha.conf.default](resources/nha.conf.default).

Start the service and enable it for future boots:

```bash
sudo systemctl enable --now nha
```

Check the service state and recent logs:

```bash
systemctl status nha
journalctl -u nha -e
```

The service requires the TPM resource manager device (`/dev/tpmrm0`) and the CPU/runtime capabilities listed above. If startup fails, check the journal output for missing TPM access, unsupported CPU features, or perf API permission issues.

## Executing Automated Tests

The `test/nha_tests` binary implements automated integration/component tests via the GTest framework. Build the test binary first, if not yet done so:

```bash
./ci/build.sh
```

Then run the test suite, usually as root:

```bash
sudo ./ci/test.sh
```

The dependency `googletest` is automatically downloaded by `CMake`. Only an internet connection is required on top of the above build dependencies.

The tests start their own `nha` process on `127.0.0.1:7872`. Stop any already running `nha` service first if it uses the same port:

```bash
sudo systemctl stop nha
```

> **Important:** These tests can only run successfully on real machines equipped with TPM hardware (`/dev/tpmrm0`). The `nha` daemon requires access to the TPM device and relies on CPU-specific features such as RDTSC for monotonic timing. Running the tests in virtual machines, containers, or environments without TPM hardware will result in all tests failing.
