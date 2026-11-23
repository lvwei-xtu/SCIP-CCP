#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

# Run from the repository root. Generate one solver command per line for a table;
# execute the commands separately, for example through a cluster submission script.
usage() {
  cat <<'HELP'
To generate the commands for running the computational experiments in a given table, please run:

bash scripts/gen_commands.sh TABLE=<T> TABLE<T>.sh

where `<T>` is chosen from `{1, 2, 3, 4, 5, 6}` and denotes the table number, and `TABLE<T>.sh` denotes the shell script to which the generated commands are written. 
The methods, instances, and time limit are selected according to the table:

| Table | Methods | Instances | Time limit (seconds) |
| --- | --- | --- | --- |
| 1 | `BASE`, `BASE+DI`, `BASE+DB+OPF` | CCRP, CCMPP, CCLS | 14400 |
| 2 | `BASE+DB`, `BASE+sDI`, `BASE+DI` | CCRP, CCMPP, CCLS | 14400 |
| 3 | `BASE+DB`, `BASE+DB+OPF` | CCRP, CCMPP, CCLS | 14400 |
| 4 | `BASE+DB+OPF`, `BASE+DB+EOPF` | CCRP, CCMPP, CCLS | 86400 |
| 5 | `BASE`, `BASE+sDI`, `BASE+DB`, `BASE+DB+OPF` | CCMPP with dimensions 10, 20, 30, 50, 100, 150 | 14400 |
| 6 | `G-BASE`, `G-BASE+sDI`, `BASE+DB+OPF` | CCRP, CCMPP, CCLS | 14400 |

For Tables 1–4 and 6, the CCMPP dimensions are 10, 20, and 30. Table 6 includes the Julia/Gurobi commands and the SCIP reruns; all three methods should be run on the same hardware. 
The Julia/Gurobi installation instructions are given in [Gurobi-Julia/README.md](Gurobi-Julia/README.md).

For example:

# Generate the commands for Table 1.
bash scripts/gen_commands.sh TABLE=1 TABLE1.sh

HELP
}

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

TABLE_NUMBER=""
COMMAND_FILE=""
for arg in "$@"; do
  case "$arg" in
    -h|--help) usage; exit 0 ;;
    TABLE=*)
      [[ -z "$TABLE_NUMBER" ]] || die 'specify TABLE only once'
      TABLE_NUMBER="${arg#TABLE=}"
      [[ -n "$TABLE_NUMBER" ]] || die 'TABLE requires a value'
      ;;
    *=*|-*) die "unsupported argument: $arg; use --help for usage" ;;
    *)
      [[ -z "$COMMAND_FILE" ]] || die 'specify only one output file'
      COMMAND_FILE="$arg"
      ;;
  esac
done
[[ -n "$COMMAND_FILE" ]] || die 'specify an output file; use --help for usage'

# Match the comparisons and instance subsets used by scripts/CCP.awk.
PROBLEM_LIST=(CCRP CCMPP CCLS)
TIME_LIMIT=14400
case "$TABLE_NUMBER" in
  1) METHOD_LIST=(BASE BASE+DI BASE+DB+OPF) ;;
  2) METHOD_LIST=(BASE+DB BASE+sDI BASE+DI) ;;
  3) METHOD_LIST=(BASE+DB BASE+DB+OPF) ;;
  4) METHOD_LIST=(BASE+DB+OPF BASE+DB+EOPF); TIME_LIMIT=86400 ;;
  5) METHOD_LIST=(BASE BASE+sDI BASE+DB BASE+DB+OPF); PROBLEM_LIST=(CCMPP) ;;
  6) METHOD_LIST=(G-BASE G-BASE+sDI BASE+DB+OPF) ;;
  *) die 'TABLE must be one of 1, 2, 3, 4, 5, 6' ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
ROOT="$(cd "$ROOT" && pwd)"
EXE="${EXE:-$ROOT/build/ccp}"
JULIA="${JULIA:-julia}"
SETTINGS_REL="settings/generated/table-$TABLE_NUMBER"

if [[ "$TABLE_NUMBER" == 6 ]]; then
  [[ -f "$ROOT/Gurobi-Julia/solve.jl" && -f "$ROOT/Gurobi-Julia/Project.toml" ]] \
    || die 'Table 6 requires Gurobi-Julia/solve.jl and Project.toml'
fi

mkdir -p "$(dirname "$COMMAND_FILE")"
TEMP_OUTPUT="$(mktemp "${COMMAND_FILE}.tmp.XXXXXX")"

# Keep the original settings intact and isolate the limits for different tables.
prepare_settings() {
  local source="$1" target="$2"
  [[ -f "$source" ]] || die "missing setting: $source"
  mkdir -p "$(dirname "$target")"
  awk -v limit="$TIME_LIMIT" '
    /^[[:space:]]*limits\/time[[:space:]]*=/ {
      print "limits/time = " limit; found = 1; next
    }
    { print }
    END { if (!found) print "limits/time = " limit }
  ' "$source" > "$target"
}

shopt -s nullglob
generated=0
for prob in "${PROBLEM_LIST[@]}"; do
  case "$prob" in
    CCRP) ext=ccrp; SUFFIXES=(1 15 2) ;;
    CCMPP) ext=ccmpp; SUFFIXES=(05 1 2) ;;
    CCLS) ext=ccls; SUFFIXES=(05 1 2) ;;
  esac
  [[ -d "$ROOT/data/${prob}Data" ]] || die "missing data directory for $prob"
  DATA_FILES=("$ROOT/data/${prob}Data/"*."$ext")
  [[ ${#DATA_FILES[@]} -gt 0 ]] || die "no .$ext instances found"

  for method in "${METHOD_LIST[@]}"; do
    solver=scip
    setting="$method"
    result_dir="$ROOT/results-scip/$prob/$method"
    case "$method" in
      G-*)
        solver=gurobi
        setting="${method#G-}"
        result_dir="$ROOT/results-gurobi/$prob/$setting"
        ;;
      BASE+DB+OPF)
        if [[ "$TABLE_NUMBER" == 6 ]]; then
          result_dir="$result_dir/14400-table6"
        else
          result_dir="$result_dir/$TIME_LIMIT"
        fi
        ;;
    esac
    mkdir -p "$result_dir"

    for suffix in "${SUFFIXES[@]}"; do
      case "$suffix" in
        05) eps=0.05 ;; 1) eps=0.1 ;; 15) eps=0.15 ;; 2) eps=0.2 ;;
      esac
      if [[ "$solver" == scip ]]; then
        setting_rel="$SETTINGS_REL/${prob}Setting/${setting}${suffix}.set"
        prepare_settings "$ROOT/settings/${prob}Setting/${setting}${suffix}.set" "$ROOT/$setting_rel"
      fi

      for data_file in "${DATA_FILES[@]}"; do
        data_base="${data_file##*/}"
        if [[ "$prob" == CCMPP ]]; then
          # Tables 1--4 and 6 use m=10,20,30; Table 5 also uses m=50,100,150.
          case "${data_base%%-*}" in
            10|20|30) ;;
            50|100|150) [[ "$TABLE_NUMBER" == 5 ]] || continue ;;
            *) continue ;;
          esac
        fi
        data_rel="data/${prob}Data/$data_base"
        log_base="$result_dir/${data_base}-${setting}-${eps}"
        if [[ "$solver" == scip ]]; then
          COMMAND=("$EXE" -f "$data_rel" -s "$setting_rel")
        else
          COMMAND=("$JULIA" --project=Gurobi-Julia Gurobi-Julia/solve.jl
            --instance "$data_rel" --setting "$setting" --epsilon "$eps"
            --time-limit "$TIME_LIMIT" --threads 1 --mip-gap 0)
        fi
        printf '%q ' "${COMMAND[@]}" >> "$TEMP_OUTPUT"
        printf '> %q 2> %q\n' "$log_base.out" "$log_base.err" >> "$TEMP_OUTPUT"
        generated=$((generated + 1))
      done
    done
  done
done
[[ "$generated" -gt 0 ]] || die 'no commands generated'
chmod +x "$TEMP_OUTPUT"
mv "$TEMP_OUTPUT" "$COMMAND_FILE"
printf 'Generated %d commands for Table %s in %s\n' "$generated" "$TABLE_NUMBER" "$COMMAND_FILE"
