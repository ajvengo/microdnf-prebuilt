"""Bzlmod extension for declaring prebuilt microdnf binary repositories via http_file."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("//microdnf:integrities.bzl", "BASE_URL", "INTEGRITIES")

def _microdnf_prebuilt_extension_impl(module_ctx):
    for name, integrity in INTEGRITIES.items():
        http_file(
            name = name,
            url = BASE_URL + "/" + name,
            integrity = integrity,
            executable = True,
        )

microdnf_prebuilt_extension = module_extension(
    implementation = _microdnf_prebuilt_extension_impl,
)
