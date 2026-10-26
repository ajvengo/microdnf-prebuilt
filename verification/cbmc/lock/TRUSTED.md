# Trusted base of the CBMC proofs in this directory

"Verified" means: verified against the statements in the harnesses (`h_*.c`), with the model
below, within the stated bounds. It does not mean the code is bug-free.

## Tools

- CBMC 6.11.0 (cbmc-6.11.0, prebuilt `ubuntu-22.04-cbmc-6.11.0-Linux.deb`, unpacked into a private dir; SAT backend MiniSat).
- Checks on every job: `--bounds-check --pointer-check --memory-leak-check --signed-overflow-check
  --unsigned-overflow-check --conversion-check --div-by-zero-check --pointer-overflow-check
  --undefined-shift-check --unwinding-assertions` (so an exhausted `--unwind` is a failure, not a silent cut-off).
- Frama-C was not available on the host and not needed; no Docker image was used.

## What is proved on shipped text, and what is hand-copied

`extract.py` prints the shipped function verbatim from `dnf/lock.c` / `dnf/lock-main.c` at run time
(the harness `#include`s it), so a changed body is re-verified automatically and a renamed
function fails the run. Extracted functions: `split_option_values`, `lock_use_best` (lock.c);
`basearch_for_arch`, `find_solve_arch`, `process_global_option` (lock-main.c).

Three lines of `run_lock` (join of the split list, the `[0]` emptiness test) are reproduced by
`h_split.c` (`g_strjoinv (",", ...)`, `res[0]`); `run.sh` greps `dnf/lock.c` for the exact call lines
(`check_copy`) and fails if they change.

## Model of GLib (`glib_model.h`) - assumptions

| Function | Model | Assumption |
|---|---|---|
| `g_malloc`, `g_new0`, `g_free`, `g_strdup`, `g_strndup` | `malloc`/`calloc`/`free`, exact fit, never NULL; `g_new0` overflow aborts (`assume`) | GLib aborts on OOM and on `n*size` overflow; no NULL return |
| allocation counter | `model_live` incremented/decremented by the above; harnesses assert 0 at exit | CBMC's `--memory-leak-check` only tracks the last allocation, so the counter is the real leak check. Memory not routed through `g_malloc` (oracle copies, scratch arrays inside the model) is plain `malloc` and not counted |
| `g_strsplit`, `g_strsplit_set` | re-implemented from the documented contract (`""` -> `{NULL}`, `"a,"` -> `{"a","",NULL}`, empty tokens kept, `max_tokens=-1` unbounded) | the real implementation matches the documentation |
| `g_strstrip` (`g_strchug`+`g_strchomp`) | C-locale `isspace` set `" \t\n\v\f\r"`, in-place, `memmove` | `g_ascii_isspace` is exactly this set |
| `g_strjoinv`, `g_strconcat`, `g_build_path` | `g_strconcat` handles exactly two parts; `g_build_path` is `a SEP b` with no separator collapsing | only validity of the returned string is checked, not its exact content (`g_build_path`) |
| `g_str_has_prefix/suffix`, `g_strsplit*`, `g_strchug` ... with NULL | an **assertion failure**, where GLib would log a CRITICAL and return | a NULL reaching these is a caller bug and must be visible |
| `GPtrArray` | exact-fit `pdata`, `g_ptr_array_free(a, FALSE)` returns `pdata` without freeing elements, `add` fails above `UINT_MAX` | real GLib over-allocates and (for non null-terminated arrays) does not NUL-terminate `pdata` either; exact fit is stricter |
| `g_auto`, `g_autofree`, `g_autoptr(GPtrArray)`, `g_steal_pointer` | `__attribute__((cleanup))` with the matching free | CBMC 6.11 ignores `__attribute__((cleanup))` (checked), so `extract.py` inserts the explicit `model_cleanup_*` calls at every `return` and scope-closing `}`, in reverse declaration order: the same scope-exit semantics as GLib's `glib-autocleanups.h` |
| `GOptionContext` (`h_solvearch.c` only) | `parse` may fail, may set each `G_OPTION_ARG_STRING` entry to an arbitrary string (replacing the old one) or leave it, may consume any number of arguments; UTF-8 conversion errors folded into "fails" | parse never writes outside its `arg_data` targets and never modifies strings of `argv` |

## Stubs of libdnf / libc (checks the callers pass valid arguments, nothing more)

- `dnf_conf_main_get_option("best", &prio, NULL)`: returns NULL or a fresh string, any priority from the
  `DnfConfPriority` enum (values copied from libdnf 0.7x headers: 0,10,20,...,80); caller owns the string.
- `dnf_context_get_best()`: nondeterministic boolean.
- `dnf_context_set_{repos_dir,vars_dir,cache_dir,solv_dir,lock_dir,platform_module,install_root,release_ver}`,
  `dnf_conf_add_setopt`, `dnf_context_set_config_file_path`: record the call and read every argument string;
  `g_slist_append` returns its list (the `--enablerepo/--disablerepo` branches are not exercised).
- `setenv`, `dnf_context_set_arch`, `dnf_context_setup` are not modelled: the `main()` glue around
  `find_solve_arch` is not verified.

## Bounds (the proofs are bounded model checking, complete only within them)

| Harness | Bound | `--unwind` |
|---|---|---|
| `split` | 0..2 option occurrences (or NULL), each 0..4 arbitrary non-NUL bytes | 14 |
| `split-alpha` | 0..2 occurrences, each 0..8 bytes over `{a,b,',',' ','\t'}` | 22 |
| `basearch` | arch of 0..7 arbitrary non-NUL bytes | 12 |
| `solvearch` | argc 1..3; `--archlist`/`--forcearch` values of 0..6 arbitrary bytes | 16 |
| `usebest` | all boolean/priority combinations (exhaustive) | 8 |
| `setopt-0..7` | one proof per family (`cachedir`, `reposdir`, `varsdir`, `module_platform_id`, `excludepkgs`, `best`, `features.excludepkgs`, arbitrary text), each `<name>=` + 0..3 arbitrary bytes | 26 |

`--unwinding-assertions` passing means no execution longer than the bound exists for these input sizes;
nothing is claimed for longer strings or more occurrences (loops are over `strlen` and the token
count, which scale linearly; the alphabet run goes to length 8 to cover separator-heavy shapes).

## Out of scope

libdnf/libsolv behaviour, `lock_recompute_modular_filtering` and `lock_goal_describe_problems`
(C++ over libdnf's `Goal`/`ModulePackageContainer`; only syntactic review: the `hotfix_repos`
vector outlives the call it is passed to, and every `catch` path sets `error` and returns FALSE),
the `--nobest` selector loop in `run_lock` (every resource there is a libdnf object), concurrency,
the `main()` option-table glue, behaviour with `argc` near `INT_MAX` (`g_new0 (gchar *, argc + 1)`
cannot overflow for a real process: argc <= ARG_MAX/2), callers violating preconditions, dead code.

## Solver and parallelism (trusted base)

- SAT backend: MiniSat (CBMC 6.11.0 default). Compared with CaDiCaL (`--sat-solver cadical`) on `setopt-0`:
  MiniSat 1.17 GiB / 16 s, CaDiCaL 2.95 GiB / 46 s; MiniSat kept. Kissat is not installed.
- Run with 16 parallel jobs (limit raised by the user from 8; sum of caps 96 GiB < 200 GiB), each under
  `ulimit -v 6 GiB` and `timeout 600`. Measured peaks: see README.
- `-DMODEL_RELAXED` (setopt, solvearch only): model-created strings get a fixed capacity (`MODEL_STR_CAP`) instead of an
  exact-fit symbolic size, `memcpy` is a byte loop, and `g_strsplit*` is a one-scan implementation bounded by
  `MODEL_MAXTOK`; both bounds are asserted in the model. Consequence: an overrun of a *model-created string* by the
  shipped code would not be an out-of-bounds in these two harnesses (the shipped code only reads them,
  NUL-terminated); arrays returned by the splits and `g_new0` blocks stay exact-fit.
