#!/usr/bin/env bash
# dnf_lock feature tests against the synthetic "features" repository:
# --installroot, excludepkgs, module streams, best/nobest, rich and weak deps.
set -euo pipefail

DNF_LOCK="$1"
REPOMD="$2"          # .../features-repo/repodata/repomd.xml
INSTALLROOT_FILE="$3" # .../installroot/var/lib/rpm/rpmdb.sqlite

abspath() { (cd "$(dirname "$1")" && echo "$(pwd -P)/$(basename "$1")"); }
REPO_DIR="$(dirname "$(dirname "$(abspath "${REPOMD}")")")"
INSTALLROOT_SRC="$(dirname "$(dirname "$(dirname "$(dirname "$(abspath "${INSTALLROOT_FILE}")")")")")"

WORK="${TEST_TMPDIR:-$(mktemp -d)}/lock_features"
rm -rf "${WORK}"
mkdir -p "${WORK}/repos.d" "${WORK}/cache"
cat > "${WORK}/repos.d/features.repo" <<REPO
[features]
name=features
baseurl=file://${REPO_DIR}
enabled=1
gpgcheck=0
REPO

# Writable copy of the parent image root: librpm opens the sqlite rpmdb in WAL mode.
fresh_installroot() {
  rm -rf "${WORK}/installroot"
  cp -RL "${INSTALLROOT_SRC}" "${WORK}/installroot"
  chmod -R u+w "${WORK}/installroot"
  echo "${WORK}/installroot"
}

FAILURES=0
OUT="${WORK}/out.json"
ERR="${WORK}/err.txt"

run_lock() {
  "${DNF_LOCK}" --reposdir="${WORK}/repos.d" --setopt=cachedir="${WORK}/cache" \
    --releasever=9 --archlist=x86_64,noarch -o "${OUT}" "$@" 2>"${ERR}"
}

nevras() {
  python3 -c 'import json,sys; print(" ".join(sorted(p["nevra"] for p in json.load(open(sys.argv[1]))["packages"])))' "${OUT}"
}

# expect_lock "description" "expected sorted NEVRAs" dnf_lock-args...
expect_lock() {
  local desc="$1" expected="$2"
  shift 2
  if ! run_lock "$@"; then
    echo "FAIL: ${desc}: dnf_lock exited non-zero"
    cat "${ERR}"
    FAILURES=$((FAILURES + 1))
    return
  fi
  local got
  got="$(nevras)"
  if [ "${got}" != "${expected}" ]; then
    echo "FAIL: ${desc}"
    echo "  expected: ${expected}"
    echo "  got:      ${got}"
    FAILURES=$((FAILURES + 1))
  else
    echo "ok: ${desc}"
  fi
}

# expect_error "description" "stderr substring" dnf_lock-args...
expect_error() {
  local desc="$1" needle="$2"
  shift 2
  if run_lock "$@"; then
    echo "FAIL: ${desc}: dnf_lock succeeded, expected an error"
    FAILURES=$((FAILURES + 1))
  elif ! grep -qF -- "${needle}" "${ERR}"; then
    echo "FAIL: ${desc}: error does not mention '${needle}'"
    cat "${ERR}"
    FAILURES=$((FAILURES + 1))
  else
    echo "ok: ${desc}"
  fi
}

# expect_json "description" "python expression over d (the lockfile)"
expect_json() {
  if python3 -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if ($2) else 1)" "${OUT}"; then
    echo "ok: $1"
  else
    echo "FAIL: $1"
    cat "${OUT}"
    FAILURES=$((FAILURES + 1))
  fi
}

echo "--- installroot (seeded rpmdb: libfoo-1.0, oldtool-1.0, rich-cond-1.0, condhost-1.0)"
expect_lock "default resolves the full closure" \
  "app-1.0-1.noarch libfoo-2.0-1.noarch oldtool-1.0-1.noarch optional-x-1.0-1.noarch" app
expect_lock "installroot locks only the transaction, upgrading libfoo" \
  "app-1.0-1.noarch libfoo-2.0-1.noarch" app --installroot="$(fresh_installroot)"
expect_json "upgrade records the installed package it replaces" \
  '[p.get("replaces") for p in d["packages"] if p["name"] == "libfoo"] == [["libfoo-1.0-1.noarch"]]'
expect_json "requested lists the requested name" 'd["requested"] == ["app"]'
expect_lock "already installed request is a no-op" "" oldtool --installroot="$(fresh_installroot)"
expect_json "no-op lock still lists the request" 'd["requested"] == ["oldtool"] and d["packages"] == []'
expect_lock "relative installroot path" \
  "app-1.0-1.noarch libfoo-2.0-1.noarch" app \
  --installroot="$(python3 -c 'import os,sys; print(os.path.relpath(sys.argv[1]))' "$(fresh_installroot)")"
mkdir -p "${WORK}/scratchroot/usr"
expect_lock "installroot without an rpmdb behaves like an empty image" \
  "app-1.0-1.noarch libfoo-2.0-1.noarch oldtool-1.0-1.noarch optional-x-1.0-1.noarch" \
  app --installroot="${WORK}/scratchroot"
expect_lock "conditional Requires of an installed package is kept" \
  "cond-extra-1.0-1.noarch cond-trigger-1.0-1.noarch" cond-trigger --installroot="$(fresh_installroot)"
expect_lock "no installed package, no conditional Requires" \
  "cond-trigger-1.0-1.noarch" cond-trigger

echo "--- weak dependencies"
expect_lock "Recommends and Supplements are installed by default" \
  "weak-rec-1.0-1.noarch weak-sup-1.0-1.noarch weakapp-1.0-1.noarch" weakapp
expect_lock "install_weak_deps=False skips them" \
  "weakapp-1.0-1.noarch" weakapp --setopt=install_weak_deps=False
expect_lock "weak deps survive --exclude pruning of unrelated packages" \
  "weak-rec-1.0-1.noarch weak-sup-1.0-1.noarch weakapp-1.0-1.noarch" weakapp --exclude=oldtool
expect_lock "--exclude prunes a weak dependency" \
  "weak-sup-1.0-1.noarch weakapp-1.0-1.noarch" weakapp --exclude=weak-rec
expect_lock "weak dep is installed without an installroot" \
  "newapp-1.0-1.noarch optional-x-1.0-1.noarch" newapp
expect_lock "weak dep declined by the installed set is not pulled (exclude_from_weak_autodetect)" \
  "newapp-1.0-1.noarch" newapp --installroot="$(fresh_installroot)"
expect_lock "exclude_from_weak_autodetect=False pulls it again" \
  "newapp-1.0-1.noarch optional-x-1.0-1.noarch" newapp --installroot="$(fresh_installroot)" \
  --setopt=exclude_from_weak_autodetect=False

echo "--- rich dependencies"
expect_lock "(a if b) without b, (b or c)" "rich-b-1.0-1.noarch richapp-1.0-1.noarch" richapp
expect_lock "(a if b) with b requested" \
  "rich-a-1.0-1.noarch rich-b-1.0-1.noarch rich-cond-1.0-1.noarch richapp-1.0-1.noarch" richapp rich-cond
expect_lock "(a if b) with b installed in the installroot" \
  "rich-a-1.0-1.noarch rich-b-1.0-1.noarch richapp-1.0-1.noarch" richapp --installroot="$(fresh_installroot)"

echo "--- best / nobest"
expect_error "best (default) fails when the newest candidate is broken" \
  "nothing provides missing-lib needed by bestapp-2.0-1.noarch" bestapp
expect_error "--best is explicit default" "missing-lib" bestapp --best
expect_lock "--nobest falls back to an older candidate" "bestapp-1.0-1.noarch" bestapp --nobest
expect_lock "--setopt=best=False falls back too" "bestapp-1.0-1.noarch" bestapp --setopt=best=False
expect_error "--best and --nobest conflict" "not allowed" bestapp --best --nobest

echo "--- excludepkgs"
expect_lock "solver sees the newest ca-certificates by default" \
  "ca-certificates-2026.2.92_v9.0.319-1.el9.noarch certuser-1.0-1.noarch" certuser
expect_lock "--excludepkgs glob hides the release cut-off" \
  "ca-certificates-2025.2.80_v9.0.305-1.el9.noarch certuser-1.0-1.noarch" \
  certuser --excludepkgs='ca-certificates-2026.2.92_v9.0.319-*'
expect_lock "--setopt=excludepkgs= glob" \
  "ca-certificates-2025.2.80_v9.0.305-1.el9.noarch certuser-1.0-1.noarch" \
  certuser --setopt='excludepkgs=ca-certificates-2026.2.92_v9.0.319-*'
expect_lock "--setopt=<repo>.excludepkgs= glob" \
  "ca-certificates-2025.2.80_v9.0.305-1.el9.noarch certuser-1.0-1.noarch" \
  certuser --setopt='features.excludepkgs=ca-certificates-2026.2.92_v9.0.319-*'
expect_lock "--excludepkgs NEVRA without arch" "expat-2.7.2-1.el10.x86_64" expat --excludepkgs=expat-2.7.3-1.el10_2.7
expect_lock "--excludepkgs list" "expat-2.7.2-1.el10.x86_64" expat --excludepkgs=nothing-matches,expat-2.7.3-1.el10_2.7
expect_error "excluding every candidate leaves nothing to lock" "No packages matched" \
  expat --excludepkgs='expat-*'

echo "--- modules (mysql:8.4 is modular, mysql 8.0 is not)"
expect_lock "modular packages are hidden by default" "mysql-8.0.44-1.el9.x86_64" mysql
expect_lock "--module-enable with module_platform_id" \
  "mysql-8.4.7-1.module+el9.7.0+90001+e2e00084.x86_64" \
  mysql --module-enable=mysql:8.4 --setopt=module_platform_id=platform:el9
expect_error "--module-enable needs a platform" "module(platform:el9)" mysql --module-enable=mysql:8.4
expect_lock "--module-enable with the installroot's os-release PLATFORM_ID" \
  "mysql-8.4.7-1.module+el9.7.0+90001+e2e00084.x86_64" \
  mysql --module-enable=mysql:8.4 --installroot="$(fresh_installroot)"
ROOT="$(fresh_installroot)"
mkdir -p "${ROOT}/etc/dnf/modules.d"
printf '[mysql]\nname=mysql\nstream=8.4\nprofiles=\nstate=enabled\n' > "${ROOT}/etc/dnf/modules.d/mysql.module"
expect_lock "stream enabled in the installroot's modules.d" \
  "mysql-8.4.7-1.module+el9.7.0+90001+e2e00084.x86_64" mysql --installroot="${ROOT}"
if [ -n "$(ls "${ROOT}/etc/dnf/modules.d" | grep -v '^mysql.module$' || true)" ]; then
  echo "FAIL: dnf_lock wrote module state into the installroot"
  FAILURES=$((FAILURES + 1))
fi

if [ "${FAILURES}" -ne 0 ]; then
  echo "${FAILURES} dnf_lock feature check(s) failed"
  exit 1
fi
echo "dnf_lock feature tests passed"
