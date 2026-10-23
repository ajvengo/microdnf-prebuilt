"""Implementation of rpm_lock rule."""

load("//microdnf/private:providers.bzl", "RpmLockFileInfo")

def _common_dir(files, short):
    """Longest common parent directory of `files` (the directory itself for a tree artifact)."""
    if not files:
        return ""
    paths = [f.short_path if short else f.path for f in files]
    if len(files) == 1:
        return paths[0] if files[0].is_directory else paths[0].rpartition("/")[0]
    common = paths[0].split("/")[:-1]
    for p in paths[1:]:
        parts = p.split("/")[:-1]
        n = 0
        for a, b in zip(common, parts):
            if a != b:
                break
            n += 1
        common = common[:n]
    return "/".join(common)

def _solver_args(ctx):
    """dnf_lock flags shaping the solve (dnf semantics), shared by build and run."""
    args = []
    for pattern in ctx.attr.excludepkgs:
        args.append("--excludepkgs=" + pattern)
    for spec in ctx.attr.module_enable:
        args.append("--module-enable=" + spec)
    if ctx.attr.module_platform_id:
        args.append("--setopt=module_platform_id=" + ctx.attr.module_platform_id)
    if ctx.attr.nobest:
        args.append("--nobest")
    if not ctx.attr.install_weak_deps:
        args.append("--setopt=install_weak_deps=False")
    for opt in ctx.attr.setopt:
        args.append("--setopt=" + opt)
    return args

def _rpm_lock_impl(ctx):
    out = ctx.outputs.out
    if not out:
        out = ctx.actions.declare_file(ctx.label.name + ".lock.json")

    build_script = ctx.actions.declare_file(ctx.label.name + "_build.sh")
    runner_script = ctx.actions.declare_file(ctx.label.name + "_run.sh")

    repo_conf = ctx.file.repo_config
    lock_tool = ctx.executable.lock_tool
    repo_files = ctx.files.repo_files

    # Determine relative repo directory by finding the parent of repodata
    repo_rel_dir = ""
    for f in repo_files:
        if "/repodata/" in f.path or f.path.endswith("/repodata"):
            parts = f.path.split("/repodata")
            repo_rel_dir = parts[0]
            break
    if not repo_rel_dir and repo_files:
        repo_rel_dir = repo_files[0].dirname

    installroot_files = ctx.files.installroot
    installroot_dir = _common_dir(installroot_files, short = False)
    installroot_short = _common_dir(installroot_files, short = True)

    # Script for hermetic bazel build
    build_content = """#!/usr/bin/env bash
set -euo pipefail

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

REPOS_DIR="${TMP_DIR}/repos.d"
mkdir -p "${REPOS_DIR}"

if grep -q '@REPO_DIR@' "%s"; then
  sed "s|@REPO_DIR@|${PWD}/%s|g" "%s" > "${REPOS_DIR}/repo.repo"
else
  cp "%s" "${REPOS_DIR}/repo.repo"
fi

CACHE_DIR="${TMP_DIR}/cache"
mkdir -p "${CACHE_DIR}"

INSTALLROOT_ARGS=()
if [ -n "%s" ]; then
  # Writable copy: librpm opens the sqlite rpmdb in WAL mode.
  cp -RL "%s" "${TMP_DIR}/installroot"
  chmod -R u+w "${TMP_DIR}/installroot"
  INSTALLROOT_ARGS+=(--installroot="${TMP_DIR}/installroot")
fi

"%s" \\
  --reposdir="${REPOS_DIR}" \\
  --setopt=cachedir="${CACHE_DIR}" \\
  ${INSTALLROOT_ARGS[@]+"${INSTALLROOT_ARGS[@]}"} \\
  "$@"
""" % (repo_conf.path, repo_rel_dir, repo_conf.path, repo_conf.path, installroot_dir, installroot_dir, lock_tool.path)

    ctx.actions.write(
        output = build_script,
        content = build_content,
        is_executable = True,
    )

    arch_str = ",".join(ctx.attr.arch)

    args = ctx.actions.args()
    args.add("--output=" + out.path)
    args.add("--archlist=" + arch_str)
    args.add("--releasever=" + ctx.attr.releasever)

    for exc in ctx.attr.exclude:
        args.add("--exclude=" + exc)

    solver_args = _solver_args(ctx)
    args.add_all(solver_args)

    for pkg in ctx.attr.packages:
        args.add(pkg)

    inputs = depset(
        direct = [repo_conf] + repo_files + installroot_files,
        transitive = [],
    )

    ctx.actions.run(
        mnemonic = "RpmLock",
        progress_message = "Generating RPM lockfile %s" % out.short_path,
        executable = build_script,
        arguments = [args],
        inputs = inputs,
        tools = [lock_tool],
        outputs = [out],
    )

    # Runner script for 'bazel run' updating the lockfile in source tree
    out_dest = ctx.attr.out.name if ctx.attr.out else ctx.label.name + ".lock.json"
    run_content = """#!/usr/bin/env bash
set -euo pipefail

DEST="${BUILD_WORKSPACE_DIRECTORY}/%s/%s"
TMP_OUT="$(mktemp)"
trap 'rm -f "${TMP_OUT}"' EXIT

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"; rm -f "${TMP_OUT}"' EXIT

REPOS_DIR="${TMP_DIR}/repos.d"
mkdir -p "${REPOS_DIR}"

CONF_PATH="%s"
REPO_REL="%s"

if grep -q '@REPO_DIR@' "${CONF_PATH}"; then
  sed "s|@REPO_DIR@|${PWD}/${REPO_REL}|g" "${CONF_PATH}" > "${REPOS_DIR}/repo.repo"
else
  cp "${CONF_PATH}" "${REPOS_DIR}/repo.repo"
fi

CACHE_DIR="${TMP_DIR}/cache"
mkdir -p "${CACHE_DIR}"

INSTALLROOT_ARGS=()
if [ -n "%s" ]; then
  cp -RL "%s" "${TMP_DIR}/installroot"
  chmod -R u+w "${TMP_DIR}/installroot"
  INSTALLROOT_ARGS+=(--installroot="${TMP_DIR}/installroot")
fi

"%s" \\
  --reposdir="${REPOS_DIR}" \\
  --setopt=cachedir="${CACHE_DIR}" \\
  ${INSTALLROOT_ARGS[@]+"${INSTALLROOT_ARGS[@]}"} \\
  --output="${TMP_OUT}" \\
  --archlist="%s" \\
  --releasever="%s" \\
  %s \\
  %s \\
  %s

mkdir -p "$(dirname "${DEST}")"
cp -f "${TMP_OUT}" "${DEST}"
echo "Lockfile updated at: ${DEST}"
""" % (
        ctx.label.package,
        out_dest,
        repo_conf.short_path,
        repo_rel_dir,
        installroot_short,
        installroot_short,
        lock_tool.short_path,
        arch_str,
        ctx.attr.releasever,
        " ".join(["--exclude='%s'" % e for e in ctx.attr.exclude]),
        " ".join(["'%s'" % a for a in solver_args]),
        " ".join(["'%s'" % p for p in ctx.attr.packages]),
    )

    ctx.actions.write(
        output = runner_script,
        content = run_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [repo_conf, lock_tool] + repo_files + installroot_files)

    return [
        DefaultInfo(
            files = depset([out]),
            executable = runner_script,
            runfiles = runfiles,
        ),
        RpmLockFileInfo(
            lock_file = out,
            arch = arch_str,
            packages = ctx.attr.packages,
        ),
    ]

rpm_lock = rule(
    implementation = _rpm_lock_impl,
    doc = "Generates a hermetic RPM JSON lockfile for specified packages and dependencies.",
    attrs = {
        "arch": attr.string_list(
            mandatory = True,
            allow_empty = False,
            doc = "Mandatory target CPU architecture list (e.g. ['x86_64', 'noarch']).",
        ),
        "exclude": attr.string_list(
            default = [],
            doc = "Optional packages to exclude and prune from the dependency tree.",
        ),
        "excludepkgs": attr.string_list(
            default = [],
            doc = "NEVRA globs hidden from the solver, like dnf excludepkgs= (e.g. release cut-offs).",
        ),
        "install_weak_deps": attr.bool(
            default = True,
            doc = "Install weak dependencies (Recommends/Supplements), like dnf install_weak_deps.",
        ),
        "installroot": attr.label(
            allow_files = True,
            doc = "Parent image root (a directory or its files) whose rpmdb and modules.d the " +
                  "lock resolves on top of, like dnf --installroot. Only the transaction is locked.",
        ),
        "module_enable": attr.string_list(
            default = [],
            doc = "Module streams to enable before resolving (NAME:STREAM), like dnf module enable.",
        ),
        "module_platform_id": attr.string(
            doc = "Platform module (e.g. 'platform:el9') when no installroot provides os-release.",
        ),
        "nobest": attr.bool(
            default = False,
            doc = "Allow older candidates when the newest requested package is not installable.",
        ),
        "lock_tool": attr.label(
            default = Label("//:dnf_lock"),
            executable = True,
            cfg = "exec",
            doc = "Underlying lock tool executable.",
        ),
        "out": attr.output(
            mandatory = False,
            doc = "Output JSON lockfile name.",
        ),
        "packages": attr.string_list(
            mandatory = True,
            allow_empty = False,
            doc = "Mandatory list of top-level RPM packages to resolve.",
        ),
        "releasever": attr.string(
            mandatory = True,
            doc = "Mandatory distribution release version (e.g. '9').",
        ),
        "repo_config": attr.label(
            mandatory = True,
            allow_single_file = [".repo", ".conf"],
            doc = "Mandatory repository configuration file.",
        ),
        "setopt": attr.string_list(
            default = [],
            doc = "Extra dnf configuration overrides passed as --setopt=KEY=VALUE.",
        ),
        "repo_files": attr.label_list(
            allow_files = True,
            doc = "Local repository metadata files (e.g. repodata files, repomd.xml).",
        ),
    },
    executable = True,
)
