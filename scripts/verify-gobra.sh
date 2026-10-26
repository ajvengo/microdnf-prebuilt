#!/usr/bin/env bash
# Run the Gobra proofs under verification/gobra/ (needs Docker, linux/amd64 image).
#   scripts/verify-gobra.sh            verify every target; non-zero on any failure
#   scripts/verify-gobra.sh --negative mutate the verified code; every mutant MUST be rejected
# Exit 0 = ok, 1 = proof error (or a surviving mutant), 2 = verifier did not complete.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
image=${GOBRA_IMAGE:-ghcr.io/viperproject/gobra@sha256:5e96431b06c2e6209aa767df9153bb4cf8b2620598598f1d7d3f3a74c09c14f9}
mem=${GOBRA_MEM:-4g}
target=$root/verification/gobra/rpm2files

# gobra <dir holding main.go harness.go stubs/>: 0 verified, 1 errors found, 2 did not complete.
gobra() {
	local dir=$1 log n
	log=$(mktemp)
	docker run --rm --platform linux/amd64 --memory "$mem" --memory-swap "$mem" \
		-v "$dir":/work "$image" -i /work/main.go /work/harness.go -I /work/stubs >"$log" 2>&1 || true
	n=$(rg -o 'Gobra found [0-9]+ error' "$log" | tail -1 | rg -o '[0-9]+' || true)
	if [[ -z $n ]] || rg -q 'Exception|OutOfMemoryError|Killed' "$log"; then
		cat "$log" >&2
		rm -f "$log"
		return 2
	fi
	if ((n > 0)); then rg -A1 'Error at' "$log" >&2 || true; fi
	rm -f "$log"
	((n == 0)) || return 1
}

if [[ ${1:-} != --negative ]]; then
	gobra "$target" && echo "rpm2files: verified"
	exit
fi

fail=0
# mutant <name> <old> <new>: replace <old> by <new> in a scratch copy of main.go; Gobra must reject it.
# A mutant that still verifies means the specification is vacuous.
mutant() {
	local name=$1 old=$2 new=$3 tmp rc=0
	tmp=$(mktemp -d)
	cp -R "$target"/. "$tmp"/
	if ! rg -qUF -- "$old" "$tmp/main.go"; then
		echo "mutant '$name': needle not found" >&2
		fail=1
		rm -rf "$tmp"
		return
	fi
	sd -A -F -- "$old" "$new" "$tmp/main.go"
	gobra "$tmp" >/dev/null 2>&1 || rc=$?
	rm -rf "$tmp"
	case $rc in
	1) echo "rejected (ok): $name" ;;
	0) echo "SURVIVED (spec vacuous): $name" >&2; fail=1 ;;
	*) echo "verifier did not complete: $name" >&2; exit 2 ;;
	esac
}

mutant 'depth bound 64 -> 65' 'if depth > 64 {' 'if depth > 65 {'
mutant 'recursion without depth+1' 'resolveMember(members, target, visited, depth+1)' 'resolveMember(members, target, visited, depth)'
mutant 'drop the not-found check' $'\tif !ok {\n\t\treturn nil, fmt.Errorf("path %s not found in archive", name)\n\t}\n' ''
mutant 'drop the leading-slash trim in cleanTarPath' $'\tp = strings.TrimPrefix(p, "/")\n' ''
exit $fail
