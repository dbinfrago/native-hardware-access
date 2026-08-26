<!--
SPDX-FileCopyrightText: Copyright DB InfraGO AG
SPDX-License-Identifier: Apache-2.0
-->

Contributing
============

Thanks for your interest in this project. Contributions are welcome.

Please keep discussions respectful and constructive. If you found a bug, have an improvement idea, or want to propose a feature, [open an issue] first for larger changes. Small fixes can be sent directly; to do so, [open a pull request].

Before opening a pull request, make sure your change is focused, tested as far as possible, and documented where needed.

Developing
----------

Clone the repository and build locally:

```bash
git clone https://github.com/dbinfrago/native-hardware-access
cd native-hardware-access
```

This project uses CMake and C++17.

### Prerequisites

Install at least the following dependencies:

- `cmake` (>= 3.25)
- `build-essential`
- `pkg-config`
- `libssl-dev`
- `libsystemd-dev`
- `libtss2-dev`
- `debhelper-compat` (= 13) for Debian packaging

The test tool `nhac` requires a third-party header that is intentionally not tracked in this git repository. The helper scripts below download this file automatically. If you run CMake directly, download the header before building:

```bash
mkdir -p third_party/picohash
curl -sL -o third_party/picohash/picohash.h \
  https://raw.githubusercontent.com/sethcall/picohash/refs/heads/master/picohash.h
echo "e8ab833db1470350596d8d766311d6dfddf805af6abc95e51ce3109567167de9  third_party/picohash/picohash.h" | sha256sum -c -
```

> The helper scripts verify the downloaded header with the same SHA-256 checksum shown above. Set `PICOHASH_URL` if the build should use an internal mirror instead of the public URL.

### Build

Use the CI helper script (recommended):

```bash
./ci/build.sh
```

Or run CMake directly:

```bash
cmake -B build -S .
cmake --build build --parallel
```

Testing
-------

Build the test binary first, if not yet done so:

```bash
./ci/build.sh
```

Run tests via:

```bash
sudo ./ci/test.sh
```

Important constraints:

- The integration tests require real TPM hardware (`/dev/tpmrm0`).
- The NHA daemon depends on CPU features such as invariant TSC/RDTSCP.
- Running tests in containers/VMs without TPM is expected to fail.
- Depending on environment and permissions, tests may require root.
- The tests start their own `nha` process on `127.0.0.1:7872`; stop any already running `nha` service first if it uses the same port:

```bash
sudo systemctl stop nha
```

If you cannot run hardware-dependent tests locally, document this in the pull request and include the build output you verified.

Debian Packaging
----------------

To build the Debian package locally, use the CI packaging helper:

```bash
./ci/package.sh
```

This script downloads and verifies `picohash.h`, runs `dpkg-buildpackage -us -uc`, and writes the generated package artifacts to `deb_output/` by default. Set `DEB_DIR` to choose a different output directory, or `PICOHASH_URL` to use a different source for the third-party header.

The Debian packaging build requires the packaging-related prerequisites listed above, including `debhelper-compat`, `cmake`, `pkg-config`, `build-essential`, `libssl-dev`, `libsystemd-dev`, and `libtss2-dev`.

Code Style
----------

General style expectations for C/C++ code:

- Keep changes minimal and scoped to the issue.
- Follow the existing formatting and naming conventions in the touched files.
- Add comments only where intent is not obvious from code.
- Preserve SPDX license headers in source files.

For documentation:

- Keep architecture and protocol docs in `doc/` in sync with code changes.
- Update `README.md` when build/test behavior or prerequisites change.

Quality Controls
----------------

Before opening a pull request, verify:

1. The project builds successfully.
2. Tests were run where possible, or limitations are clearly stated.
3. Documentation is updated for behavior/interface changes.
4. No unrelated refactoring was mixed into the change.

License and Attribution
-----------------------

Contributors must keep licensing information up to date for all changes.

- Keep SPDX headers correct in newly added or modified source and documentation files.
- Add third-party code, snippets, or copied files only if they are not publicly available from another source. Ensure that they have correct license attribution.
- Update Debian packaging metadata in `debian/copyright` when the set of distributed files or attributions changes.
- Do not add files with incompatible or unknown licensing terms.
- If in doubt, open an issue or ask maintainers before submitting the change.

Commit Message Format
---------------------

Use clear, imperative commit messages.

Conventional commits are recommended, for example:

- `fix: handle invalid TLV length in responder`
- `docs: clarify relationships in class diagram`

Pull Requests
-------------

A good pull request should include:

- The problem statement and motivation.
- A concise summary of the implemented solution.
- Test/build evidence and environment details.
- Notes about hardware assumptions (TPM, CPU, permissions), if relevant.

For larger changes, split work into reviewable commits.

Security Reporting
------------------

Do not disclose security-sensitive issues publicly in detail before maintainers can triage and mitigate. For suspected vulnerabilities, contact maintainers via the repository owner channel first.

[open an issue]: https://github.com/dbinfrago/native-hardware-access/issues
[open a pull request]: https://github.com/dbinfrago/native-hardware-access/pulls

---

## Available CI/CD Pipelines

### For GitLab

#### Overview
The project includes a GitLab CI/CD pipeline (`.gitlab-ci.yml`) that automatically compiles the project, builds the Debian package and runs the test suite. The pipeline consists of the following stages:

1. **package** – Compiles the project and creates the Debian package (`.deb`) using `dpkg-buildpackage`.
2. **buildAndDoTests** – Compiles the project with CMake (including the test suite) and runs the integration tests.
3. **release** – Creates a GitLab Release with the `.deb` package attached (only runs when a Git tag is pushed).

> **Note on tests:** The automated tests are **integration tests** that start the `nha` binary as a daemon and communicate with it via the `nhac` client. Successful execution requires **real TPM hardware** (`/dev/tpmrm0`) as well as specific CPU features (RDTSC), which are not available in CI containers. The tests are therefore only executed in the CI for completeness and are allowed to fail (`allow_failure: true`). Meaningful test execution is only possible on the target hardware.

#### Creating a Release

To create a release, push a Git tag. This can be done via the command line:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Alternatively, a tag can be created directly in GitLab under **Code → Tags → New tag**.

The pipeline will run all stages as usual. Once the build succeeds, the **release** stage automatically creates a GitLab Release named after the tag and attaches the `.deb` package as a downloadable asset.
