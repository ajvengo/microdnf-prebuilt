# microdnf-prebuilt

Bazel repository capable of building `microdnf`, `rpm2archive`, and `lock` so they can be used hermetically across platforms.

## Architecture

- **Upstream Tracking**: Tracks upstream `microdnf` as a Bzlmod module extension (`//bazel:extensions.bzl%microdnf_extension`) using `git_repository` pinned to the latest upstream commit (`c6a9aa90902b5f050825003398d3a8fd4542e533`).
- **Hermetic Toolchains**: Clang/LLVM, Python 3.14 via `aspect_rules_py`, and Remote Execution (RBE) via `toolchains_buildbuddy`.
- **Multi-Platform Support**: Cross-compiles for `linux_amd64`, `linux_arm64`, `osx_amd64`, and `osx_arm64`.

## Features Added

1. **`rpm2archive`**:
   - Creates a deduplicated layer archive (`.tar`, `.tar.gz`, `.tar.zst`) from a list of RPM packages for container layers.
   - Handles conflicts where later RPM arguments override earlier ones, and ensures directory entries are created exactly once.
   - Available both as a standalone tool (`//:rpm2archive`) and as a `microdnf` subcommand plugin (`microdnf rpm2archive`).

2. **`lock`**:
   - Resolves package dependencies via `libdnf`'s solver and generates a JSON lockfile with SRI integrity checksums and dependency trees.
   - Available both as a standalone tool (`//:lock`) and as a `microdnf` subcommand plugin (`microdnf lock`).

3. **Hermetic Host Portability**:
   - Built-in fallback for non-RPM systems where `/usr/lib/rpm/rpmrc` is absent.
   - Fallback cache and repo directories for non-root / unprivileged execution.

## Building

```bash
# Build standalone binaries for host
bazel build //:microdnf //:rpm2archive //:lock

# Build for all supported platforms
bazel build //:microdnf_all //:rpm2archive_all //:lock_all
```
