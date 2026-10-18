"""Public API for microdnf-prebuilt."""

load("//microdnf:toolchain.bzl", _MicrodnfInfo = "MicrodnfInfo", _microdnf_toolchain = "microdnf_toolchain")

MicrodnfInfo = _MicrodnfInfo
microdnf_toolchain = _microdnf_toolchain
