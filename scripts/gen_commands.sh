#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C
export LANG=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
RESULT_NAME="${RESULT_NAME:-results-0707}"
RESULT_ROOT="${RESULT_ROOT:-$ROOT/$RESULT_NAME}"
COMMAND_FILE="${1:-$SCRIPT_DIR/${RESULT_NAME}.commands.sh}"

EXE="${EXE:-}"
PROBLEMS_SELECTED="${PROBLEMS:-}"
METHODS_SELECTED="${METHODS:-}"
MODE="${MODE:-main}"
OPF_LIMIT="${OPF_LIMIT:-}"

usage() {
  cat <<'EOF'
Usage:
  bash lsf/gen_commands.sh [COMMAND_FILE]

Purpose:
  Generate CCP test commands into COMMAND_FILE.  The script does not submit or
  execute jobs.

Modes:
  MODE=main   default; generate BASE, BASE+DI, BASE+sDI, BASE+DB, BASE+DB+OPF
              and write BASE+DB+OPF outputs under BASE+DB+OPF/14400.
  MODE=long   generate only BASE+DB+OPF and BASE+DB+EOPF; write BASE+DB+OPF
              outputs under BASE+DB+OPF/86400.  Before using this mode, manually
              set limits/time = 86400 in the BASE+DB+OPF setting files.
  MODE=all    generate all six settings; BASE+DB+OPF uses OPF_LIMIT or 14400.

Environment overrides:
  ROOT          repo root; default is the parent directory of lsf/
  RESULT_NAME   result directory name; default results-0706
  RESULT_ROOT   full result directory; default <ROOT>/<RESULT_NAME>
  EXE           executable path used in generated commands; default auto-detect
  PROBLEMS      comma-separated subset: CCRP,CCMPP,CCLS
  METHODS       comma-separated subset overriding MODE
  MODE          main, long, or all
  OPF_LIMIT     output subfolder for BASE+DB+OPF, usually 14400 or 86400

Examples:
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

  PROBLEMS=CCRP  bash scripts/gen_commands.sh scripts/CCRP.sh
  PROBLEMS=CCMPP bash scripts/gen_commands.sh scripts/CCMPP.sh
  PROBLEMS=CCLS  bash scripts/gen_commands.sh scripts/CCLS.sh

  PROBLEMS=CCRP MODE=long bash scripts/gen_commands.sh scripts/CCRP-long.sh
  PROBLEMS=CCMPP MODE=long bash scripts/gen_commands.sh scripts/CCMPP-long.sh
  PROBLEMS=CCLS MODE=long bash scripts/gen_commands.sh scripts/CCLS-long.sh
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ -z "$EXE" ]]; then
  if [[ -x "$ROOT/build/ccp" ]]; then
    EXE="./build/ccp"
  elif [[ -x "$ROOT/build/bin/examples/ccp" ]]; then
    EXE="./build/bin/examples/ccp"
  else
    EXE="./build/ccp"
  fi
fi

if [[ "$EXE" = /* ]]; then
  COMMAND_EXE="$EXE"
else
  COMMAND_EXE="$ROOT/${EXE#./}"
fi

PROBLEM_LIST=(CCRP CCMPP CCLS)

case "$MODE" in
  main)
    DEFAULT_METHODS=(BASE BASE+DI BASE+sDI BASE+DB BASE+DB+OPF)
    [[ -n "$OPF_LIMIT" ]] || OPF_LIMIT=14400
    ;;
  long)
    DEFAULT_METHODS=(BASE+DB+OPF BASE+DB+EOPF)
    [[ -n "$OPF_LIMIT" ]] || OPF_LIMIT=86400
    ;;
  all)
    DEFAULT_METHODS=(BASE BASE+DI BASE+sDI BASE+DB BASE+DB+OPF BASE+DB+EOPF)
    [[ -n "$OPF_LIMIT" ]] || OPF_LIMIT=14400
    ;;
  *)
    printf 'ERROR: unknown MODE: %s\n' "$MODE" >&2
    usage >&2
    exit 1
    ;;
esac

if [[ -n "$METHODS_SELECTED" ]]; then
  IFS=',' read -r -a METHOD_LIST <<< "$METHODS_SELECTED"
else
  METHOD_LIST=("${DEFAULT_METHODS[@]}")
fi

csv_contains() {
  local csv="$1"
  local item="$2"
  [[ -z "$csv" ]] && return 0
  case ",$csv," in
    *,"$item",*) return 0 ;;
    *) return 1 ;;
  esac
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
    *) printf '%s' "$1" ;;
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

shell_quote() {
  printf '%q' "$1"
}

mkdir -p "$SCRIPT_DIR" "$RESULT_ROOT" "$(dirname "$COMMAND_FILE")"
: > "$COMMAND_FILE"

generated=0
missing=0

for prob in "${PROBLEM_LIST[@]}"; do
  csv_contains "$PROBLEMS_SELECTED" "$prob" || continue

  data_dir_rel="data/$(problem_data_dir "$prob")"
  setting_dir_rel="settings/$(problem_setting_dir "$prob")"
  ext="$(problem_extension "$prob")"
  data_dir="$ROOT/$data_dir_rel"
  setting_dir="$ROOT/$setting_dir_rel"

  if [[ ! -d "$data_dir" ]]; then
    printf 'ERROR: data directory not found: %s\n' "$data_dir" >&2
    exit 1
  fi
  if [[ ! -d "$setting_dir" ]]; then
    printf 'ERROR: setting directory not found: %s\n' "$setting_dir" >&2
    exit 1
  fi

  while IFS= read -r -d '' data_file; do
    data_rel="${data_file#$ROOT/}"
    data_base="$(basename "$data_file")"

    for method in "${METHOD_LIST[@]}"; do
      method_prefix="$(method_setting_prefix "$method")"
      result_method_dir="$(method_result_dir "$prob" "$method")"
      mkdir -p "$result_method_dir"

      for suffix in $(problem_suffixes "$prob"); do
        eps="$(suffix_to_eps "$suffix")"
        setting_file="$setting_dir/${method_prefix}${suffix}.set"
        setting_rel="${setting_file#$ROOT/}"

        if [[ ! -f "$setting_file" ]]; then
          printf 'WARN: missing setting: %s\n' "$setting_file" >&2
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
          "$(shell_quote "$err_file")" >> "$COMMAND_FILE"

        generated=$((generated + 1))
      done
    done
  done < <(find "$data_dir" -type f -name "*.${ext}" -print0 | sort -z)
done

chmod +x "$COMMAND_FILE"

printf 'INFO root: %s\n' "$ROOT"
printf 'INFO mode: %s\n' "$MODE"
printf 'INFO OPF output folder: %s\n' "$OPF_LIMIT"
printf 'INFO executable in commands: %s\n' "$COMMAND_EXE"
printf 'INFO result directory: %s\n' "$RESULT_ROOT"
printf 'INFO command file: %s\n' "$COMMAND_FILE"
printf 'INFO generated commands: %d\n' "$generated"
printf 'INFO missing settings: %d\n' "$missing"

if [[ "$missing" -gt 0 ]]; then
  exit 2
fi
