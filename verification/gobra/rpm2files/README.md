# Gobra proof: rpm2files path and symlink resolution

Targets `cleanTarPath` and `resolveMember` in `rpm2files/main.go`. Trusted base and assumptions: [TRUSTED.md](TRUSTED.md).

## Theorems (from the fuzz targets in `rpm2files/main_test.go`)

| Fuzz target | Function | Proved (Gobra) | Only fuzzed |
|---|---|---|---|
| `FuzzCleanTarPath` | `cleanTarPath` | terminates, no panic, result never starts with `/` (given the `path.Clean` stub) | no `./` prefix; idempotence outside the root |
| `FuzzResolveMember` | `resolveMember` | for any `members`/`visited` meeting the preconditions: no nil dereference or nil-map write, terminates (`decreases 65 - depth`), at most `65 - depth` new `visited` entries, i.e. depth <= 64; `err == nil` implies a non-nil entry; permissions on `members` are returned | the returned entry is never a symlink/hardlink; depth 65 is rejected |
| `FuzzParseBool`, `FuzzParseArgsFile` | `parseBool`, `parseArgsFile` | nothing | no panic, `true` only for the documented spellings, no half-parsed specs on error |

`harness.go` is a verified caller that mirrors `main()`/`FuzzResolveMember` (fresh `visited`, depth 0, a real entry in `members`); it shows the preconditions are satisfiable and the depth bound is usable.

## Finding (not a production bug)

`FuzzCleanTarPath` shows `cleanTarPath` is not idempotent for the root: every spelling of the absolute root (`/`, `//`, `/.`, `/..`) maps to `""`, and `cleanTarPath("")` is `"."`. Both denote a directory, never a file member, so extraction of regular files is unaffected. Pinned by `TestCleanTarPathRoot`. No patch: normalising `""` to `"."` would make a symlink to `/` resolve to the root directory entry where it is today a "not found" error, which widens accepted inputs for no benefit.

## Patches to production code

None. The proof needed no change to `rpm2files/main.go`.

## Commands

```
scripts/verify-gobra.sh             # proof: "rpm2files: verified"
scripts/verify-gobra.sh --negative  # every mutant below must be "rejected (ok)"
go test ./verification/...          # drift test: extracted copy == shipped bodies
go test -run '^$' -fuzz FuzzResolveMember -fuzztime 60s ./rpm2files   # likewise the other targets
```

## Negative controls (mutation applied to a scratch copy of `main.go`; Gobra must reject)

| Mutant | Result |
|---|---|
| `if depth > 64 {` -> `if depth > 65 {` | rejected |
| recursive call `depth+1` -> `depth` | rejected (termination) |
| drop the `if !ok { ... not found }` check | rejected (nil entry dereference) |
| drop `strings.TrimPrefix(p, "/")` in `cleanTarPath` | rejected (postcondition) |

The drift test itself was checked by changing `depth > 64` to `depth > 63` in `rpm2files/main.go`: `TestExtractedMatchesShipped/resolveMember` fails.

Note: `gofmt` rewrites `//@` into `// @` and breaks the annotations. Never run it on files under `verification/gobra/rpm2files/` (they carry `//go:build gobra`, so `golangci-lint` and `go build` skip them).
