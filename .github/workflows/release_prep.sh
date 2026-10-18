#!/usr/bin/env bash
set -euo pipefail

TAG="${1:-${GITHUB_REF_NAME:-}}"
if [ -z "${TAG}" ]; then
  echo "Error: Tag name must be provided as first argument or GITHUB_REF_NAME" >&2
  exit 1
fi

echo "Processing release tag: ${TAG}"

SHORT_SHA=""
BUILD_ID=""
FULL_SHA=""

if [[ "${TAG}" =~ microdnf-prebuilt-v([0-9a-fA-F]+)\.bcr\.([0-9]+) ]]; then
  SHORT_SHA="${BASH_REMATCH[1]}"
  BUILD_ID="${BASH_REMATCH[2]}"
elif [[ "${TAG}" =~ v([0-9a-fA-F]+)\.bcr\.([0-9]+) ]]; then
  SHORT_SHA="${BASH_REMATCH[1]}"
  BUILD_ID="${BASH_REMATCH[2]}"
else
  # Generic tag fallback
  SHORT_SHA="${TAG#v}"
  BUILD_ID="1"
fi

echo "Resolving upstream microdnf commit from short SHA: ${SHORT_SHA}..."

# 1. Try git ls-remote against upstream repository
if [ -n "${SHORT_SHA}" ]; then
  FULL_SHA=$(git ls-remote https://github.com/rpm-software-management/microdnf.git | awk '{print $1}' | grep -i "^${SHORT_SHA}" | head -n 1 || true)
fi

# 2. Try GitHub API if not found via ls-remote heads
if [ -z "${FULL_SHA}" ] && [ -n "${SHORT_SHA}" ]; then
  FULL_SHA=$(curl -sSL "https://api.github.com/repos/rpm-software-management/microdnf/commits/${SHORT_SHA}" | jq -r '.sha // empty' || true)
fi

# 3. Fallback to SHORT_SHA if remote resolution could not find full 40-char hash
if [ -z "${FULL_SHA}" ]; then
  FULL_SHA="${SHORT_SHA}"
fi

echo "Resolved upstream microdnf SHA: ${FULL_SHA} (short: ${SHORT_SHA}, build: ${BUILD_ID})"

# Update bazel/extensions.bzl to point to the resolved upstream git commit
if [ -f "bazel/extensions.bzl" ]; then
  echo "Updating bazel/extensions.bzl to commit: ${FULL_SHA}"
  sed -i -E "s/commit = \"[0-9a-fA-F]+\"/commit = \"${FULL_SHA}\"/" bazel/extensions.bzl
  git diff bazel/extensions.bzl || true
fi

# Annotate in GitHub Actions workflow if running on CI
if [ -n "${GITHUB_OUTPUT:-}" ]; then
  echo "tag_name=${TAG}" >> "${GITHUB_OUTPUT}"
  echo "upstream_full_sha=${FULL_SHA}" >> "${GITHUB_OUTPUT}"
  echo "upstream_short_sha=${SHORT_SHA}" >> "${GITHUB_OUTPUT}"
  echo "build_id=${BUILD_ID}" >> "${GITHUB_OUTPUT}"
fi

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  cat >> "${GITHUB_STEP_SUMMARY}" << EOF
### Release Annotation for \`${TAG}\`
- **Upstream microdnf Commit**: [\`${FULL_SHA}\`](https://github.com/rpm-software-management/microdnf/commit/${FULL_SHA})
- **Short SHA**: \`${SHORT_SHA}\`
- **Build ID**: \`${BUILD_ID}\`
EOF
fi

echo "::notice title=Upstream microdnf SHA::Resolved ${SHORT_SHA} to ${FULL_SHA} (build ${BUILD_ID})"
echo "Release preparation complete."
