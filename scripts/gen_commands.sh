#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C
export LANG=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
RESULT_NAME="${RESULT_NAME:-results-0709}"
RESULT_ROOT="${RESULT_ROOT:-$ROOT/$RESULT_NAME}"
COMMAND_FILE="${1:-$SCRIPT_DIR/${RESULT_NAME}.commands.sh}"
EXE="${EXE:-}"
PROBLEMS_SELECTED="${PROBLEMS:-}"
METHODS_SELECTED="${METHODS:-}"
MODE="${MODE:-main}"
OPF_LIMIT="${OPF_LIMIT:-}"

usage() {
  cat <<EOF
Usage: [PROBLEMS=CCRP,CCMPP,CCLS] [METHODS=BASE,...] [MODE=main|exact|all|ccmpp03] bash scripts/gen_commands.sh OUTPUT.sh

Generate shell command files for SCIP-CCP experiments. Commands are generated only;
they are not submitted to LSF.

MODE=main  generates BASE, BASE+DI, BASE+sDI, BASE+DB, BASE+DB+OPF with 14400s output path.
MODE=exact generates BASE+DB+OPF with 86400s output path and BASE+DB+EOPF. If METHODS is
           not set, commands are split into PROBLEM-BASE+DB+OPF-86400.sh and
           PROBLEM-BASE+DB+EOPF.sh under the output directory. For CCMPP, m=50,
           m=100, and m=150 instances are skipped in this mode.
MODE=all   generates all settings with the normal 14400s BASE+DB+OPF output path.
MODE=ccmpp03 generates epsilon=0.3 commands for CCMPP only, using BASE, BASE+DI,
             BASE+sDI, BASE+DB, and BASE+DB+OPF. All commands are written to
             the requested output file. Instances with m=50, 100, or 150 are
             skipped.

Environment variables:
  ROOT         Repository root. Default: parent of scripts/.
  RESULT_NAME  Result folder name under ROOT. Default: results-0709.
  RESULT_ROOT  Full result root. Default: ROOT/RESULT_NAME.
  EXE          Solver executable. Default: ROOT/build/ccp.
  PROBLEMS     Comma-separated subset of CCRP, CCMPP, CCLS. Default: all.
  METHODS      Comma-separated subset of BASE, BASE+DI, BASE+sDI, BASE+DB, BASE+DB+OPF, BASE+DB+EOPF.
  MODE         main, exact, all, or ccmpp03. Default: main.

Examples:
  PROBLEMS=CCRP  bash scripts/gen_commands.sh scripts/CCRP.sh
  PROBLEMS=CCMPP bash scripts/gen_commands.sh scripts/CCMPP.sh
  PROBLEMS=CCLS  bash scripts/gen_commands.sh scripts/CCLS.sh

  PROBLEMS=CCRP  METHODS=BASE bash scripts/gen_commands.sh scripts/CCRP-BASE.sh
  PROBLEMS=CCRP  METHODS=BASE+DI bash scripts/gen_commands.sh scripts/CCRP-BASE+DI.sh
  PROBLEMS=CCRP  METHODS=BASE+sDI bash scripts/gen_commands.sh scripts/CCRP-BASE+sDI.sh
  PROBLEMS=CCRP  METHODS=BASE+DB bash scripts/gen_commands.sh scripts/CCRP-BASE+DB.sh
  PROBLEMS=CCRP  METHODS=BASE+DB+OPF bash scripts/gen_commands.sh scripts/CCRP-BASE+DB+OPF.sh

  PROBLEMS=CCMPP  METHODS=BASE bash scripts/gen_commands.sh scripts/CCMPP-BASE.sh
  PROBLEMS=CCMPP  METHODS=BASE+DI bash scripts/gen_commands.sh scripts/CCMPP-BASE+DI.sh
  PROBLEMS=CCMPP  METHODS=BASE+sDI bash scripts/gen_commands.sh scripts/CCMPP-BASE+sDI.sh
  PROBLEMS=CCMPP  METHODS=BASE+DB bash scripts/gen_commands.sh scripts/CCMPP-BASE+DB.sh
  PROBLEMS=CCMPP  METHODS=BASE+DB+OPF bash scripts/gen_commands.sh scripts/CCMPP-BASE+DB+OPF.sh

  PROBLEMS=CCLS  METHODS=BASE bash scripts/gen_commands.sh scripts/CCLS-BASE.sh
  PROBLEMS=CCLS  METHODS=BASE+DI bash scripts/gen_commands.sh scripts/CCLS-BASE+DI.sh
  PROBLEMS=CCLS  METHODS=BASE+sDI bash scripts/gen_commands.sh scripts/CCLS-BASE+sDI.sh
  PROBLEMS=CCLS  METHODS=BASE+DB bash scripts/gen_commands.sh scripts/CCLS-BASE+DB.sh
  PROBLEMS=CCLS  METHODS=BASE+DB+OPF bash scripts/gen_commands.sh scripts/CCLS-BASE+DB+OPF.sh

  PROBLEMS=CCRP MODE=exact bash scripts/gen_commands.sh scripts/CCRP-exact.sh
  PROBLEMS=CCMPP MODE=exact bash scripts/gen_commands.sh scripts/CCMPP-exact.sh
  PROBLEMS=CCLS MODE=exact bash scripts/gen_commands.sh scripts/CCLS-exact.sh

  MODE=ccmpp03 bash scripts/gen_commands.sh scripts/CCMPP-epsilon03.sh

EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ -z "$EXE" ]]; then
  EXE="$ROOT/build/ccp"
fi

COMMAND_EXE="$EXE"

case "$MODE" in
  main)
    DEFAULT_METHODS=(BASE BASE+DI BASE+sDI BASE+DB BASE+DB+OPF)
    OPF_LIMIT="${OPF_LIMIT:-14400}"
    ;;
  exact)
    DEFAULT_METHODS=(BASE+DB+OPF BASE+DB+EOPF)
    OPF_LIMIT="${OPF_LIMIT:-86400}"
    ;;
  all)
    DEFAULT_METHODS=(BASE BASE+DI BASE+sDI BASE+DB BASE+DB+OPF BASE+DB+EOPF)
    OPF_LIMIT="${OPF_LIMIT:-14400}"
    ;;
  ccmpp03)
    DEFAULT_METHODS=(BASE BASE+DI BASE+sDI BASE+DB BASE+DB+OPF)
    OPF_LIMIT="${OPF_LIMIT:-14400}"
    if [[ -n "$PROBLEMS_SELECTED" && "$PROBLEMS_SELECTED" != "CCMPP" ]]; then
      printf 'ERROR: MODE=ccmpp03 only supports PROBLEMS=CCMPP\n' >&2
      exit 1
    fi
    PROBLEMS_SELECTED="CCMPP"
    ;;
  *)
    printf 'ERROR: unsupported MODE=%s\n' "$MODE" >&2
    usage >&2
    exit 1
    ;;
esac

ALL_PROBLEMS=(CCRP CCMPP CCLS)

if [[ -n "$METHODS_SELECTED" ]]; then
  IFS=',' read -r -a METHOD_LIST <<< "$METHODS_SELECTED"
else
  METHOD_LIST=("${DEFAULT_METHODS[@]}")
fi

EXACT_SPLIT=0
if [[ "$MODE" == "exact" && -z "$METHODS_SELECTED" ]]; then
  EXACT_SPLIT=1
fi

COMMAND_DIR="$(dirname "$COMMAND_FILE")"
COMMAND_FILES=()

add_command_file_once() {
  local file="$1"
  local existing
  for existing in "${COMMAND_FILES[@]:-}"; do
    [[ "$existing" == "$file" ]] && return 0
  done
  mkdir -p "$(dirname "$file")"
  : > "$file"
  COMMAND_FILES+=("$file")
}

exact_command_file() {
  local prob="$1"
  local method="$2"
  case "$method" in
    BASE+DB+OPF) printf '%s/%s-BASE+DB+OPF-%s.sh' "$COMMAND_DIR" "$prob" "$OPF_LIMIT" ;;
    BASE+DB+EOPF) printf '%s/%s-BASE+DB+EOPF.sh' "$COMMAND_DIR" "$prob" ;;
    *) printf '%s' "$COMMAND_FILE" ;;
  esac
}

command_file_for() {
  local prob="$1"
  local method="$2"
  if [[ "$EXACT_SPLIT" -eq 1 ]]; then
    exact_command_file "$prob" "$method"
  else
    printf '%s' "$COMMAND_FILE"
  fi
}

method_in_list() {
  local want="$1"
  local method
  for method in "${METHOD_LIST[@]}"; do
    [[ "$method" == "$want" ]] && return 0
  done
  return 1
}

csv_contains() {
  local csv="$1"
  local item="$2"
  local token
  [[ -z "$csv" ]] && return 0
  IFS=',' read -r -a tokens <<< "$csv"
  for token in "${tokens[@]}"; do
    [[ "$token" == "$item" ]] && return 0
  done
  return 1
}

problem_data_dir() {
  case "$1" in
    CCRP) printf 'CCRPData' ;;
    CCMPP) printf 'CCMPPData' ;;
    CCLS) printf 'CCLSData' ;;
    *) return 1 ;;
  esac
}

problem_setting_dir() {
  case "$1" in
    CCRP) printf 'CCRPSetting' ;;
    CCMPP) printf 'CCMPPSetting' ;;
    CCLS) printf 'CCLSSetting' ;;
    *) return 1 ;;
  esac
}

problem_extension() {
  case "$1" in
    CCRP) printf 'ccrp' ;;
    CCMPP) printf 'ccmpp' ;;
    CCLS) printf 'ccls' ;;
    *) return 1 ;;
  esac
}

problem_suffixes() {
  if [[ "$MODE" == "ccmpp03" ]]; then
    [[ "$1" == "CCMPP" ]] || return 1
    printf '3'
    return
  fi

  case "$1" in
    CCRP) printf '15 1 2' ;;
    CCMPP|CCLS) printf '05 1 2' ;;
    *) return 1 ;;
  esac
}

suffix_to_eps() {
  case "$1" in
    05) printf '0.05' ;;
    15) printf '0.15' ;;
    1) printf '0.1' ;;
    2) printf '0.2' ;;
    3) printf '0.3' ;;
    *) return 1 ;;
  esac
}

method_setting_prefix() {
  case "$1" in
    BASE) printf 'BASE' ;;
    BASE+DI) printf 'BASE+DI' ;;
    BASE+sDI) printf 'BASE+sDI' ;;
    BASE+DB) printf 'BASE+DB' ;;
    BASE+DB+OPF) printf 'BASE+DB+OPF' ;;
    BASE+DB+EOPF) printf 'BASE+DB+EOPF' ;;
    *) return 1 ;;
  esac
}

method_result_dir() {
  local prob="$1"
  local method="$2"
  if [[ "$method" == "BASE+DB+OPF" ]]; then
    printf '%s/%s/%s/%s' "$RESULT_ROOT" "$prob" "$method" "$OPF_LIMIT"
  else
    printf '%s/%s/%s' "$RESULT_ROOT" "$prob" "$method"
  fi
}

skip_instance_for_mode() {
  local prob="$1"
  local data_base="$2"
  local m="${data_base%%-*}"

  if [[ ("$MODE" == "exact" || "$MODE" == "ccmpp03") && "$prob" == "CCMPP" ]]; then
    case "$m" in
      50|100|150) return 0 ;;
    esac
  fi

  return 1
}

shell_quote() {
  printf '%q' "$1"
}

update_exact_opf_settings() {
  local prob="$1"
  local setting_dir="$ROOT/settings/$(problem_setting_dir "$prob")"
  local suffix file tmp updated=0 missing=0

  if [[ ! -d "$setting_dir" ]]; then
    printf 'WARN missing %s setting directory for exact mode: %s\n' "$prob" "$setting_dir" >&2
    return 0
  fi

  for suffix in $(problem_suffixes "$prob"); do
    file="$setting_dir/BASE+DB+OPF${suffix}.set"
    if [[ ! -f "$file" ]]; then
      printf 'WARN missing %s BASE+DB+OPF setting for exact mode: %s\n' "$prob" "$file" >&2
      missing=$((missing + 1))
      continue
    fi

    tmp="$(mktemp)"
    sed 's/^limits\/time[[:space:]]*=.*/limits\/time = 86400/' "$file" > "$tmp"
    if cmp -s "$tmp" "$file"; then
      rm -f "$tmp"
    else
      mv "$tmp" "$file"
      updated=$((updated + 1))
    fi
  done

  printf 'INFO %s BASE+DB+OPF exact settings updated: %d\n' "$prob" "$updated"
  if [[ "$missing" -gt 0 ]]; then
    printf 'WARN %s BASE+DB+OPF exact settings missing: %d\n' "$prob" "$missing" >&2
  fi
}

mkdir -p "$SCRIPT_DIR" "$RESULT_ROOT" "$COMMAND_DIR"

if [[ "$EXACT_SPLIT" -eq 0 ]]; then
  add_command_file_once "$COMMAND_FILE"
fi

if [[ "$MODE" == "exact" ]] && method_in_list BASE+DB+OPF; then
  for prob in "${ALL_PROBLEMS[@]}"; do
    csv_contains "$PROBLEMS_SELECTED" "$prob" || continue
    update_exact_opf_settings "$prob"
  done
fi

generated=0
missing=0

for prob in "${ALL_PROBLEMS[@]}"; do
  csv_contains "$PROBLEMS_SELECTED" "$prob" || continue

  data_dir="$ROOT/data/$(problem_data_dir "$prob")"
  setting_dir="$ROOT/settings/$(problem_setting_dir "$prob")"
  data_dir_rel="data/$(problem_data_dir "$prob")"
  setting_dir_rel="settings/$(problem_setting_dir "$prob")"
  ext="$(problem_extension "$prob")"

  if [[ ! -d "$data_dir" ]]; then
    printf 'WARN missing data directory: %s\n' "$data_dir" >&2
    missing=$((missing + 1))
    continue
  fi
  if [[ ! -d "$setting_dir" ]]; then
    printf 'WARN missing setting directory: %s\n' "$setting_dir" >&2
    missing=$((missing + 1))
    continue
  fi

  while IFS= read -r -d '' data_file; do
    data_rel="${data_file#$ROOT/}"
    data_base="$(basename "$data_file")"

    if skip_instance_for_mode "$prob" "$data_base"; then
      continue
    fi

    for method in "${METHOD_LIST[@]}"; do
      method_prefix="$(method_setting_prefix "$method")"
      result_method_dir="$(method_result_dir "$prob" "$method")"
      target_command_file="$(command_file_for "$prob" "$method")"
      add_command_file_once "$target_command_file"
      mkdir -p "$result_method_dir"

      for suffix in $(problem_suffixes "$prob"); do
        eps="$(suffix_to_eps "$suffix")"
        setting_file="$setting_dir/${method_prefix}${suffix}.set"
        setting_rel="$setting_dir_rel/${method_prefix}${suffix}.set"

        if [[ ! -f "$setting_file" ]]; then
          printf 'WARN missing setting: %s\n' "$setting_file" >&2
          missing=$((missing + 1))
          continue
        fi

        out_file="$result_method_dir/${data_base}-${method}-${eps}.out"
        err_file="$result_method_dir/${data_base}-${method}-${eps}.err"

        printf '%s -f %s -s %s > %s 2> %s\n' \
          "$(shell_quote "$COMMAND_EXE")" \
          "$(shell_quote "$data_rel")" \
          "$(shell_quote "$setting_rel")" \
          "$(shell_quote "$out_file")" \
          "$(shell_quote "$err_file")" >> "$target_command_file"
        generated=$((generated + 1))
      done
    done
  done < <(find "$data_dir" -type f -name "*.${ext}" -print0 | sort -z)
done

for command_file in "${COMMAND_FILES[@]:-}"; do
  chmod +x "$command_file"
done

if [[ "$EXACT_SPLIT" -eq 1 ]]; then
  printf 'INFO generated %d commands into:\n' "$generated"
  for command_file in "${COMMAND_FILES[@]:-}"; do
    printf '  %s\n' "$command_file"
  done
else
  printf 'INFO generated %d commands into %s\n' "$generated" "$COMMAND_FILE"
fi
printf 'INFO result root: %s\n' "$RESULT_ROOT"
printf 'INFO executable in commands: %s\n' "$COMMAND_EXE"

if [[ "$missing" -gt 0 ]]; then
  printf 'ERROR encountered %d missing inputs\n' "$missing" >&2
  exit 2
fi
