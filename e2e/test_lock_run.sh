#!/usr/bin/env bash
# `bazel run` of an rpm_lock: the runner solves from its runfiles (the current directory, as under bazel run) and
# writes the lockfile into BUILD_WORKSPACE_DIRECTORY. It locks what the build action locks; only the url, the
# absolute path of the repository, differs.
set -euo pipefail

RUNNER="$1"
BUILT="$2"
WRITTEN="$3"

export BUILD_WORKSPACE_DIRECTORY="${TEST_TMPDIR}/workspace"
"./${RUNNER}"

python3 - "${BUILT}" "${BUILD_WORKSPACE_DIRECTORY}/${WRITTEN}" <<'PY'
import json
import sys


def without_url(path):
    with open(path) as f:
        lock = json.load(f)
    for pkg in lock["packages"]:
        del pkg["url"]
    return lock


built, written = without_url(sys.argv[1]), without_url(sys.argv[2])
assert written == built, f"bazel run wrote\n{written}\nthe build locked\n{built}"
assert written["packages"], "the lockfile locks no packages"
print("ok   bazel run writes the lockfile of the build")
PY
