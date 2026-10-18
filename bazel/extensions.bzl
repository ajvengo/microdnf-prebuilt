"""Bzlmod extension for tracking upstream microdnf."""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _microdnf_extension_impl(module_ctx):
    git_repository(
        name = "microdnf_src",
        remote = "https://github.com/rpm-software-management/microdnf.git",
        commit = "c6a9aa90902b5f050825003398d3a8fd4542e533",
        build_file = Label("//bazel:microdnf.BUILD.bazel"),
    )

microdnf_extension = module_extension(
    implementation = _microdnf_extension_impl,
)
