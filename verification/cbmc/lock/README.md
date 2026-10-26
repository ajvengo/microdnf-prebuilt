# CBMC proofs for the `lock` solver options

Scope: the option code added in `dnf/lock.c`, `dnf/lock-main.c`, `dnf/lock-repo.{h,cpp}` since fork
`main` b2ecd9a (`--excludepkgs`, `--module-enable`, `--best/--nobest`, `--forcearch`, `--setopt`
routing). Properties: no out-of-bounds access, no NULL/invalid/freed-pointer dereference, no leak
or double free, no signed/unsigned overflow or lossy conversion, plus the functional spec taken
from `e2e/test_lock_features.sh` / `test_lock_attrs.sh`. See `TRUSTED.md` for the model and bounds.

| Harness | Shipped function(s) | Spec |
|---|---|---|
| `h_split.c` | `split_option_values` (+ the `g_strjoinv` / `[0]` use in `run_lock`) | tokens stripped, non-empty, separator-free, unique, sound and complete w.r.t. the input; `split(join(split(x))) == split(x)`; no leak |
| `h_basearch.c` | `basearch_for_arch` | `i?86 -> i386`, `armv*hl -> armhfp`, identity otherwise (pointer-identical); never NULL |
| `h_solvearch.c` | `find_solve_arch` | forcearch wins; else first non-empty non-`noarch`/`src` archlist entry; else/failed parse NULL; caller can `g_free` the result once |
| `h_usebest.c` | `lock_use_best` | `--nobest` > `--best` > explicit config value > default TRUE; no leak of the option string |
| `h_setopt.c` | `process_global_option` (`--setopt` branch) | split at the first `=`; cachedir/reposdir/varsdir/module_platform_id routed; rest verbatim to `dnf_conf_add_setopt`; no `=` ignored |

Run (needs `cbmc` 6.x and `python3`; at most 8 concurrent CBMC processes, each capped at 6 GiB and 600 s; see the table at the end for measured peaks):

    CBMC=/path/to/cbmc JOBS=8 verification/cbmc/lock/run.sh            # everything
    verification/cbmc/lock/run.sh '^(split|basearch)$'                   # filter by job name

Exit 0 only if every proof passes, every witness is reachable and every negative control is rejected.

## Non-vacuity

* `w-*` jobs recompile a harness with `-DWITNESS=1`, which asserts the *negation* of an interesting
  state (arm mapping reached, forcearch/archlist branch reached, explicit `best=False` reached,
  module_platform_id/excludepkgs routing reached). CBMC must report exactly that assertion as failed.
* Preconditions are `assume`s on input shape only (string length/alphabet, argc range, boolean
  domains); there is no `assume` over a result and none outside `glib_model.h`/harness setup.

## Negative controls (`n-*` jobs)

Each mutates the extracted copy of the shipped function (never the file in the tree) with
`extract.py <file> <func> --mutate OLD NEW`; the run fails loudly if OLD is not found. CBMC must
reject every one. The mutations are the arguments of the `job n-...` lines in `run.sh`:

| Job | Mutation | Caught as |
|---|---|---|
| `n-split-empty` | drop `**tok &&` | "no empty token" |
| `n-split-dedup` | drop the duplicate lookup | "uniqueness" |
| `n-split-term` | drop the closing `g_ptr_array_add (out, NULL)` | OOB read past the list |
| `n-split-leak` | `g_auto(GStrv) tokens` -> plain pointer | leak counter |
| `n-split-dup` | store `*tok` instead of `g_strdup (*tok)` | double free / use after free |
| `n-split-strip` | drop `g_strstrip` | "no leading/trailing whitespace" |
| `n-basearch-oob` | test `arch[1] == 'i'` / read `arch[2]` | OOB read on short arch |
| `n-basearch-len` | drop `strlen (arch) == 4` | "i?86 -> i386" oracle |
| `n-solve-free` | return `opt_forcearch` instead of stealing it | double free |
| `n-solve-empty` | drop `**a &&` | "first usable archlist entry" |
| `n-solve-src` | drop the `src` exclusion | "first usable archlist entry" |
| `n-solve-uaf` | return `*a` instead of `g_strdup (*a)` | use after free |
| `n-best-nobest` | drop the `opt_nobest` early return | "--nobest wins" |
| `n-best-leak` | `g_autofree gchar *val` -> plain pointer | leak counter |
| `n-best-prio` | `prio >= DNF_CONF_DEFAULT` | "default is best" |
| `n-setopt-oob` | `if (setopt[1])` (drop the `setopt[0] &&` guard) | OOB read on `--setopt=` |
| `n-setopt-split` | `g_strsplit (value, "=", -1)` | "split at the first '='" |
| `n-setopt-plat` | pass `setopt[0]` to `set_platform_module` | "exactly the value" |

Single example, by hand:

    python3 -I extract.py ../../../dnf/lock.c split_option_values \
        --mutate 'g_auto(GStrv) tokens =' 'GStrv tokens =' > /tmp/m/split_option_values.inc
    cbmc h_split.c -I /tmp/m -I . -DL=4 --bounds-check --pointer-check --memory-leak-check \
        --signed-overflow-check --unsigned-overflow-check --conversion-check --unwind 14 --unwinding-assertions
    # VERIFICATION FAILED (leak)

## Results of the last full run (CBMC 6.11.0, MiniSat, 16 parallel jobs, 6 GiB / 600 s per process)

| Job | Result | Peak RSS | Time |
|---|---|---|---|
| `basearch`, `usebest` | proved | 0.04 / 0.02 GiB | 1 s / 0 s |
| `solvearch` (argc 1..3, values 0..6 bytes, unwind 16) | proved | 1.73 GiB | 81 s |
| `setopt-0` .. `setopt-7` (value tail 0..3 bytes, unwind 26) | proved | 2.31 - 2.42 GiB | 31 - 37 s |
| `w-*` (basearch, usebest, solvearch, setopt-cache/plat/excl) | reachable | <= 2.32 GiB | <= 72 s |
| `n-basearch-*`, `n-best-*`, `n-solve-*`, `n-setopt-*` | all rejected | <= 2.41 GiB | <= 189 s |
| `split`, `split-alpha`, `w-split-*`, `n-split-*` | **NOT COMPLETED**: hit the 600 s timeout (rc 124) | 0.7 - 1.8 GiB | 600 s |

The `split` family did not finish within 600 s after the `glib_model.h` changes made for `setopt`
(`MODEL_RELAXED`, byte-loop `memcpy`, both only active with `-DMODEL_RELAXED`); a re-run with the
default exact-fit model and `L=2..4` also exceeded 500 s, so the cause is not isolated yet (the
earlier agents' `split` runs predate these edits). Treat `split_option_values` as **unproven** until
`run.sh '^split'` is green; the host connection was lost before this could be investigated.

### SAT backend comparison (`setopt-0`, same bounds)

MiniSat (default): 1.17 GiB, 16 s. CaDiCaL (`--sat-solver cadical`): 2.95 GiB, 46 s. MiniSat is kept.
The earlier `setopt` OOMs were formula size (CBMC conversion), not the solver: CaDiCaL also ran out of 6 GiB.

### Why `setopt`/`solvearch` needed model changes

Allocations of symbolic size and `strsplit` token loops unrolled to `--unwind` made nested splits cost > 6 GiB.
With `-DMODEL_RELAXED` model-created strings have a fixed capacity (`MODEL_STR_CAP`, asserted) and
`g_strsplit*` records token boundaries in one scan (`MODEL_MAXTOK`, asserted); the split result arrays
stay exact-fit, so `n-setopt-oob` still catches the out-of-bounds read of `setopt[1]`.
