# Trusted base: rpm2files proofs

"Verified" means verified against the statements in `main.go`, under the assumptions below.

## Toolchain

- Gobra image `ghcr.io/viperproject/gobra@sha256:5e96431b06c2e6209aa767df9153bb4cf8b2620598598f1d7d3f3a74c09c14f9` (linux/amd64, run under emulation on Apple silicon)
- Z3 4.16.0, OpenJDK 21.0.12 (from that image)
- Flags: `-i /work/main.go /work/harness.go -I /work/stubs`. No `--overflow`: `int` is unbounded, which is sound here because the only arithmetic is `depth+1` with `depth <= 65` enforced by the precondition and the guard.
- Run: `scripts/verify-gobra.sh` (proof), `scripts/verify-gobra.sh --negative` (mutants must be rejected).

## Stubs (assumed, not proved; each restates documented stdlib behaviour)

| Stub | Contract assumed |
|---|---|
| `path.Clean` | result non-empty; contains no `//` (pkg.go.dev/path: "Replace multiple slashes with a single slash", empty -> ".") ; terminates |
| `path.Dir` | result non-empty (it is Clean-ed); terminates |
| `path.Join` | terminates |
| `strings.HasPrefix`, `strings.TrimPrefix` | the documented byte-index semantics; terminate |
| `strings.Literals()` | byte content of the literals `"/"` and `"./"` (Gobra does not model string-literal bytes) |
| `fmt.Errorf` | terminates, returns non-nil error |
| `archive/tar` | only `Header{Typeflag, Name, Linkname}` and `TypeLink`, `TypeSymlink` |

The stubs are `//go:build gobra` files under `stubs/`, so they never compile into the product.

## Preconditions of `resolveMember` (what callers must guarantee)

- `members` and `visited` non-nil; `0 <= depth <= 65` (production calls with 0).
- Every value in `members` is non-nil, has a non-nil `header`, and the entries and headers are pairwise distinct pointers. Production builds the map with a fresh `&tarEntry{header: hdr}` per key and `tar.Reader.Next` returns a fresh header per call, so this holds in `main()`. If `members` ever stores one entry under two keys, the proof does not apply (the code would still be safe; the proof's permission accounting is what needs distinctness).

## Extracted copy

Gobra cannot import `archive/tar`, `compress/gzip`, `zstd` or `os`, so `main.go` here holds the verified functions as a copy. It is tied to the shipped code by `../extract_test.go`: parameter lists and bodies of `cleanTarPath`, `resolveMember` and the `tarEntry` type must be identical (comments and `//@` annotations stripped). Result names (`(r string)`, `(entry, err)`) are the only allowed difference.

## Out of scope

- Everything else in `rpm2files/main.go`: `openArchive` (file and decompressor I/O), `parseArgsFile`, `parseBool`, `main` (fuzzed only: `FuzzParseArgsFile`, `FuzzParseBool`).
- Functional correctness of symlink resolution (the right entry is returned for a real archive); only non-panic, termination and the depth bound are proved. The fuzz target additionally checks the result is never a link entry.
- Concurrency (none in the verified code), memory exhaustion, callers that violate the preconditions.
- `cleanTarPath` idempotence is **not** a theorem: it is false for the root (see README).
