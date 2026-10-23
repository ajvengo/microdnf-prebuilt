# microdnf-prebuilt

Bazel repository capable of building `microdnf`, `rpm2archive`, and `dnf_lock` hermetically across platforms.

## Architecture

- **Upstream Tracking**: Tracks upstream `microdnf` as a Bzlmod module extension (`//bazel:extensions.bzl%microdnf_extension`) using `git_repository` pinned to upstream commit [`c6a9aa90902b5f050825003398d3a8fd4542e533`](https://github.com/rpm-software-management/microdnf/commit/c6a9aa90902b5f050825003398d3a8fd4542e533).
- **Hermetic Toolchains**: Hermetic Clang/LLVM, Python 3.14 via `aspect_rules_py`, and Remote Execution (RBE) via `toolchains_buildbuddy`.
- **Multi-Platform Cross-Compilation**: Supports compilation and cross-compilation for `linux_amd64`, `linux_arm64`, `osx_amd64`, and `osx_arm64`.

## Features

### 1. `rpm2archive`
Generates a single deduplicated layer archive (`.tar`, `.tar.gz`, `.tar.zst`) from a list of RPM packages for container layers (e.g. `rules_oci` / `rules_image`).
- Deduplicates file conflicts across RPMs where packages later in the command-line argument list take precedence.
- Emits parent directory entries exactly once without duplicate records.
- Available as a standalone CLI (`//:rpm2archive`) and as a `microdnf` subcommand plugin (`microdnf rpm2archive`).

```bash
# Standalone CLI
bazel run //:rpm2archive -- -o /path/to/layer.tar.zst pkg1.rpm pkg2.rpm

# Subcommand plugin
bazel run //:microdnf -- rpm2archive -o /path/to/layer.tar.gz pkg1.rpm pkg2.rpm
```

### 2. `dnf_lock`
Resolves package dependency closures and emits formatted JSON lockfiles for reproducible builds and module extensions.
- Solves complete dependency closures via `libdnf`'s solver without requiring local root privileges.
- Outputs package details: `name`, `version`, `arch`, `nevra`, remote `url`, SRI `integrity` (`sha256-...`), hex `checksum`, and `dependencies`.
- Pre-parses repository configuration flags (`--setopt=reposdir=...`, `--setopt=cachedir=...`, `--config=...`, `--releasever=...`, `--enablerepo=...`) prior to context setup.
- Available as a standalone CLI (`//:dnf_lock`) and as a `microdnf` subcommand plugin (`microdnf lock`).

```bash
# Standalone CLI
bazel run //:dnf_lock -- -o lockfile.json --setopt=reposdir=/path/to/repos.d ed bash

# Subcommand plugin
bazel run //:microdnf -- lock -o lockfile.json --setopt=reposdir=/path/to/repos.d ed bash
```

#### Resolving on top of a parent image (`--installroot`)
By default the installed set is empty and the lock lists the whole closure. Pass `--installroot=<dir>` holding
a parent image's rpmdb (`var/lib/rpm/rpmdb.sqlite` or `usr/lib/sysimage/rpm`) to resolve like `dnf install` in a
Dockerfile `RUN` on that image:
- The lock lists only the transaction: new installs and upgrades. Packages already satisfied by the image are left out.
- An upgraded package carries `"replaces": [<installed nevra>…]`; packages the transaction erases or obsoletes are
  listed in a top-level `"removed"` array. Both keys appear only when non-empty.
- Requirements of the installed packages count, e.g. glibc's `(glibc-gconv-extra if redhat-rpm-config)`.
- `etc/dnf/modules.d` (enabled module streams) and `PLATFORM_ID` from `etc/os-release` (or `usr/lib/os-release`) are read from the root.
- The directory must be writable (librpm opens the sqlite rpmdb in WAL mode) and contain `usr/`. A root with no rpmdb
  behaves like an empty image. `rpm_lock(installroot = ...)` copies the files into a writable scratch dir.

#### dnf options
| Option | Behaviour |
| --- | --- |
| `--excludepkgs=<glob,…>` / `--setopt=excludepkgs=…` | dnf `excludepkgs`: matching packages are invisible to the solver (a cut-off like `ca-certificates-2026*`). Repo-scoped `--setopt=<repo>.excludepkgs=…` also works. Unlike `--exclude`, which prunes resolved packages out of the lock graph. |
| `--module-enable=<name:stream>` | Enables a module stream before resolving, like `dnf module enable`. Streams already enabled in the installroot's `modules.d` are honoured. The platform comes from the installroot's `PLATFORM_ID` or `--setopt=module_platform_id=platform:el9`. |
| `--best` / `--nobest` | Best is the default, as in EL's `dnf.conf`: requested packages must resolve to their newest version or the lock fails. `--nobest` or `--setopt=best=False` allows older candidates. |
| `--setopt=install_weak_deps=False` | Weak dependencies (Recommends, Supplements) are installed by default; this skips them. `exclude_from_weak_autodetect` and `exclude_from_weak` are honoured. |
| `--forcearch=<arch>` | Solver architecture. It defaults to the first non-`noarch` `--archlist` entry, so an x86_64 lock resolves on an arm64 host. `$basearch` in repo files still follows the host, so use explicit-arch URLs. |

Rich dependencies (`(a if b)`, `(a or b)`) are solved by libsolv and kept in the lock graph. Any other `--setopt=<key>=<value>`
or `<repo>.<key>=<value>` goes to libdnf; a `--setopt` without `=` is an error.

`rpm_lock` exposes the same knobs as `excludepkgs`, `install_weak_deps`, `installroot`, `module_enable`,
`module_platform_id`, `nobest` and `setopt` attributes.

### 3. Hermetic Host Portability
- Automatic hermetic fallback configuration when `/usr/lib/rpm/rpmrc` is absent on host systems.
- Unprivileged user fallback redirecting caches to `~/.cache/microdnf` and repos to a fallback directory when `/etc/yum.repos.d` is missing.

## Building

```bash
# Build standalone binaries for the host
bazel build //:microdnf //:rpm2archive //:dnf_lock

# Build cross-compiled binaries for all platforms (linux_amd64, linux_arm64, osx_amd64, osx_arm64)
bazel build //:microdnf_all //:rpm2archive_all //:dnf_lock_all
```

## License

This project is licensed under the [Apache License, Version 2.0](LICENSE).

The Apache 2.0 license is a permissive open source license that allows free use, modification, reproduction, and distribution in both open source and private closed-source / commercial software projects.
