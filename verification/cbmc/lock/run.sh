#!/usr/bin/env bash
# Runs every CBMC proof, witness (non-vacuity) and negative control of this directory.
#   CBMC=/path/to/cbmc JOBS=16 verification/cbmc/lock/run.sh [name-filter-regex]
# Exit 0 only when every job behaves as its EXPECT says. Shipped functions are extracted
# verbatim from dnf/*.c at run time (extract.py), so the proofs cannot drift from the source.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
CBMC=${CBMC:-cbmc}
JOBS=${JOBS:-16}
if (( JOBS > 16 )); then echo "refusing JOBS=$JOBS: at most 16 concurrent CBMC processes (6 GiB cap each, 96 GiB worst case)"; exit 2; fi
MEMKB=${MEMKB:-6291456}   # per-process virtual-memory cap (6 GiB)
TMO=${TMO:-600}           # per-process wall-clock cap (s)
FILTER=${1:-.}
OUT=${OUT:-$(mktemp -d "${TMPDIR:-/tmp}/cbmc-lock.XXXXXX")}
mkdir -p "$OUT"

CHECKS=(--bounds-check --pointer-check --memory-leak-check --signed-overflow-check
        --unsigned-overflow-check --conversion-check --div-by-zero-check
        --pointer-overflow-check --undefined-shift-check --unwinding-assertions)

LOCK=$ROOT/dnf/lock.c
MAIN=$ROOT/dnf/lock-main.c

# job NAME HARNESS EXPECT UNWIND "DEFS" SRC:FUNC[::OLD=>NEW] ...
#   EXPECT: pass   - VERIFICATION SUCCESSFUL
#           fail   - negative control: must FAIL (any property)
#           witness- reachability witness: must fail ONLY on the property named "witness"
tmp=$OUT/results.txt; : >"$tmp"
job() {
  [[ $1 =~ $FILTER ]] || return 0
  while (( $(jobs -rp | wc -l) >= JOBS )); do wait -n; done
  run_job "$@" >>"$tmp" 2>&1 &
}

run_job() {
  local name=$1 harness=$2 expect=$3 unwind=$4 defs=$5
  shift 5
  local dir=$OUT/$name log=$OUT/$name.log
  mkdir -p "$dir"
  local spec src func mut
  for spec in "$@"; do
    src=${spec%%:*}; local rest=${spec#*:}
    func=${rest%%::*}
    mut=""
    [[ $rest == *::* ]] && mut=${rest#*::}
    local args=("$ROOT/dnf/$src" "$func")
    if [[ -n $mut ]]; then args+=(--mutate "${mut%%=>*}" "${mut#*=>}"); fi
    python3 -I "$HERE/extract.py" "${args[@]}" >"$dir/$func.inc" 2>"$log" || { echo "ERROR  $name (extract)"; cat "$log"; return 2; }
  done
  local d=()
  for x in $defs; do d+=("-D$x"); done
  [[ $expect == witness && $defs != *WITNESS=* ]] && d+=(-DWITNESS=1)
  local t0=$SECONDS
  ( ulimit -v "$MEMKB"; exec /usr/bin/time -v -o "$dir/time.txt" timeout "$TMO" "$CBMC" "$HERE/$harness" -I "$dir" -I "$HERE" "${d[@]}" "${CHECKS[@]}" --unwind "$unwind" ) >"$log" 2>&1
  local rc=$? failing
  local rss; rss=$(awk '/Maximum resident/ {printf "%.2f", $NF/1048576}' "$dir/time.txt" 2>/dev/null)
  local stat="[peak ${rss:-?} GiB, $((SECONDS - t0)) s]"
  failing=$(grep -E ': FAILURE$' "$log" | sed -E 's/^\[[^]]*\] //; s/ line ([0-9]+)/ L\1/' | sort -u | head -4 | tr '\n' ';')
  case $expect in
    pass)    [[ $rc -eq 0 ]] && echo "OK     $name  (proved) $stat" || { echo "BAD    $name  rc=$rc $failing $stat"; return 1; } ;;
    fail)    [[ $rc -eq 10 ]] && echo "OK     $name  (rejected: ${failing:0:140}) $stat" || { echo "BAD    $name  rc=$rc expected rejection"; return 1; } ;;
    witness) if [[ $rc -eq 10 ]] && ! grep -E ': FAILURE$' "$log" | grep -qv witness; then echo "OK     $name  (reachable) $stat"; else echo "BAD    $name  rc=$rc $failing $stat"; return 1; fi ;;
  esac
}

# Model allocations of symbolic size are made fixed-size (MODEL_RELAXED, see TRUSTED.md) for the
# harnesses whose nested splits otherwise need > 6 GiB.
RELAX_SETOPT="MODEL_RELAXED MODEL_STR_CAP=26 MODEL_MAXTOK=6"
RELAX_SOLVE="MODEL_RELAXED MODEL_STR_CAP=8 MODEL_MAXTOK=8"
# split_option_values: inputs of <= L=3 bytes, so a split has <= 4 tokens of <= 3 bytes (cap 4 incl. NUL).
# The properties are separate jobs (h_split.c ONLY=n) because one job holding all of them needs > 6 GiB.
SPLIT_B="L=3 NV=2 MODEL_RELAXED MODEL_STR_CAP=4 MODEL_MAXTOK=4"
# negative controls only need to show the mutant is rejected: smaller inputs, more time margin
SPLIT_N="L=2 NV=2 MODEL_RELAXED MODEL_STR_CAP=3 MODEL_MAXTOK=3"
# round trip splits the joined result a second time: a single occurrence only (2 occurrences do not fit)
SPLIT_RT="L=3 NV=1 ONLY=3 MODEL_RELAXED MODEL_STR_CAP=4 MODEL_MAXTOK=4"

# ---------------------------------------------------------------- proofs
SPLIT=lock.c:split_option_values
job split-basic    h_split.c     pass    6  "$SPLIT_B ONLY=0"              $SPLIT
job split-sound    h_split.c     pass    6  "$SPLIT_B ONLY=1 FLAT_SOUND"   $SPLIT
job split-complete h_split.c     pass    6  "$SPLIT_B ONLY=2"              $SPLIT
job split-trip     h_split.c     pass    6  "$SPLIT_RT"                    $SPLIT
job basearch       h_basearch.c  pass    12 "L=7"            lock-main.c:basearch_for_arch
job solvearch      h_solvearch.c pass    ${US:-16} "L=${LS:-6} $RELAX_SOLVE"            lock-main.c:find_solve_arch
job usebest        h_usebest.c   pass    8  ""               lock.c:lock_use_best
# --setopt: one proof per value family (SEL 0 cachedir, 1 reposdir, 2 varsdir, 3 module_platform_id,
# 4 excludepkgs, 5 best, 6 <repo>.excludepkgs, 7 arbitrary text); together they cover all eight.
for sel in 0 1 2 3 4 5 6 7; do
  job setopt-$sel h_setopt.c pass ${UT:-26} "L=${LT:-3} SEL=$sel $RELAX_SETOPT" lock-main.c:process_global_option
done

# ---------------------------------------------------------------- witnesses (non-vacuity)
job w-split-two    h_split.c     witness 6  "$SPLIT_B ONLY=0 WITNESS=1" $SPLIT
job w-split-dedup  h_split.c     witness 6  "$SPLIT_B ONLY=0 WITNESS=2" $SPLIT
job w-split-comma  h_split.c     witness 6  "$SPLIT_B ONLY=0 WITNESS=3" $SPLIT
job w-basearch     h_basearch.c  witness 12 "L=7"            lock-main.c:basearch_for_arch
job w-solvearch    h_solvearch.c witness 16 "L=6 $RELAX_SOLVE" lock-main.c:find_solve_arch
job w-usebest      h_usebest.c   witness 8  ""               lock.c:lock_use_best
job w-setopt-cache h_setopt.c    witness 26 "L=3 SEL=0 $RELAX_SETOPT"      lock-main.c:process_global_option
job w-setopt-plat  h_setopt.c    witness 26 "L=3 SEL=3 $RELAX_SETOPT"      lock-main.c:process_global_option
job w-setopt-excl  h_setopt.c    witness 26 "L=3 SEL=4 $RELAX_SETOPT"      lock-main.c:process_global_option

# ---------------------------------------------------------------- negative controls
job n-split-empty  h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::if (**tok && =>if ("
job n-split-dedup  h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::!g_ptr_array_find_with_equal_func (out, *tok, g_str_equal, NULL)=>1"
job n-split-term   h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::  g_ptr_array_add (out, NULL);
=>"
job n-split-leak   h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::g_auto(GStrv) tokens=>GStrv tokens"
job n-split-dup    h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::g_ptr_array_add (out, g_strdup (*tok))=>g_ptr_array_add (out, *tok)"
job n-split-strip  h_split.c     fail    6  "$SPLIT_N ONLY=0"            "$SPLIT::          g_strstrip (*tok);
=>"
job n-basearch-oob h_basearch.c  fail    12 "L=7"            "lock-main.c:basearch_for_arch::arch[0] == 'i' && arch[1] != '\\0'=>arch[1] == 'i' && arch[2] != '\\0'"
job n-basearch-len h_basearch.c  fail    12 "L=7"            "lock-main.c:basearch_for_arch:: && strlen (arch) == 4=>"
job n-solve-free   h_solvearch.c fail    16 "L=6 $RELAX_SOLVE"            "lock-main.c:find_solve_arch::return g_steal_pointer (&opt_forcearch);=>return opt_forcearch;"
job n-solve-empty  h_solvearch.c fail    16 "L=6 $RELAX_SOLVE"            "lock-main.c:find_solve_arch::if (**a && g_strcmp0=>if (g_strcmp0"
job n-solve-src    h_solvearch.c fail    16 "L=6 $RELAX_SOLVE"            "lock-main.c:find_solve_arch:: && g_strcmp0 (*a, \"src\") != 0=>"
job n-solve-uaf    h_solvearch.c fail    16 "L=6 $RELAX_SOLVE"            "lock-main.c:find_solve_arch::return g_strdup (*a);=>return *a;"
job n-best-nobest  h_usebest.c   fail    8  ""               "lock.c:lock_use_best::  if (opt_nobest)
    return FALSE;
=>"
job n-best-leak    h_usebest.c   fail    8  ""               "lock.c:lock_use_best::g_autofree gchar *val=>gchar *val"
job n-best-prio    h_usebest.c   fail    8  ""               "lock.c:lock_use_best::prio > DNF_CONF_DEFAULT=>prio >= DNF_CONF_DEFAULT"
job n-setopt-oob   h_setopt.c    fail    26 "L=3 SEL=7 $RELAX_SETOPT"      "lock-main.c:process_global_option::if (setopt[0] && setopt[1])=>if (setopt[1])"
job n-setopt-split h_setopt.c    fail    26 "L=3 SEL=4 $RELAX_SETOPT"      "lock-main.c:process_global_option::g_strsplit (value, \"=\", 2)=>g_strsplit (value, \"=\", -1)"
job n-setopt-plat  h_setopt.c    fail    26 "L=3 SEL=3 $RELAX_SETOPT"      "lock-main.c:process_global_option::dnf_context_set_platform_module (ctx, setopt[1]);=>dnf_context_set_platform_module (ctx, setopt[0]);"

# ---------------------------------------------------------------- hand-copied run_lock lines must still exist
check_copy() {
  if grep -qF -- "$1" "$LOCK"; then echo "OK     copy   $2"; else echo "BAD    copy   $2 (line changed in dnf/lock.c)"; return 1; fi
}

rc_all=0
wait
sort -k2 "$tmp"
grep -q '^BAD\|^ERROR' "$tmp" && rc_all=1
if [[ $FILTER == . ]]; then
  check_copy 'g_auto(GStrv) excludepkgs = split_option_values (opt_excludepkgs);' "excludepkgs = split_option_values" || rc_all=1
  check_copy 'g_autofree gchar *globs = g_strjoinv (",", excludepkgs);' "globs = g_strjoinv" || rc_all=1
  check_copy 'g_auto(GStrv) module_specs = split_option_values (opt_module_enable);' "module_specs = split_option_values" || rc_all=1
fi
echo "logs: $OUT"
exit $rc_all
