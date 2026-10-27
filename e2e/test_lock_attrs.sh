#!/usr/bin/env bash
# rpm_lock attributes excludepkgs and forcearch reach dnf_lock: each lockfile
# resolves differently from the defaults.
set -euo pipefail

python3 -c '
import json
import sys

def nevras(path):
    with open(path) as f:
        return sorted(p["nevra"] for p in json.load(f)["packages"])

def check(path, expected, what):
    got = nevras(path)
    assert got == expected, f"{what}: expected {expected}, got {got}"
    print(f"ok: {what}")

check(sys.argv[1], ["ca-certificates-2025.2.80_v9.0.305-1.el9.noarch", "certuser-1.0-1.noarch"],
      "excludepkgs hides the newer ca-certificates")
check(sys.argv[2], ["expat-2.7.3-1.el10_2.7.x86_64"], "forcearch solves for x86_64")
' "$@"
