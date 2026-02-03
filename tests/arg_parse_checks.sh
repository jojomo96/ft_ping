#!/bin/zsh
# Arg parsing boundary checks for ft_ping.
#
# All flags in src/globals.c are covered:
#   -v/--verbose, -q/--quiet, -?/--help,
#   --ttl <N>, -c/--count <N>, -i/--interval <SEC>, -s/--size <N>, -w/--timeout <N>
#
# This script supports two execution modes:
#   1) Unprivileged (e.g. macOS without sudo/cap_net_raw):
#        parse success => reaches socket() => "socket: Operation not permitted"
#   2) Privileged (e.g. Linux VM with sudo/cap_net_raw):
#        parse success => real ping output (PING header and/or replies)

set -u

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT_DIR/ft_ping"

pass=0
fail=0

say() {
  print -- "$@"
}

die() {
  say "$@"
  exit 2
}

require_bin() {
  if [[ ! -x "$BIN" ]]; then
    die "Binary not found/executable: $BIN" \
        "Run: make -C $ROOT_DIR re"
  fi
}

contains() {
  local hay="$1"
  local needle="$2"
  print -- "$hay" | grep -q -- "$needle"
}

run_cmd() {
  local -a cmd=("$BIN" "$@");
  "${cmd[@]}" 2>&1
}

expect_contains() {
  local desc="$1"; shift
  local out="$1"; shift
  local needle="$1"; shift

  if contains "$out" "$needle"; then
    say "[OK]   $desc"
    ((pass++))
  else
    say "[FAIL] $desc"
    say "       expected: $needle"
    say "       out: ${out}"
    ((fail++))
  fi
}

expect_not_contains() {
  local desc="$1"; shift
  local out="$1"; shift
  local needle="$1"; shift

  if contains "$out" "$needle"; then
    say "[FAIL] $desc"
    say "       not expected: $needle"
    say "       out: ${out}"
    ((fail++))
  else
    say "[OK]   $desc"
    ((pass++))
  fi
}

# ----- Parse-success detection -----
# macOS/unprivileged indicator
SOCKET_PERM_ERR="socket: Operation not permitted"
# privileged indicator (Linux/macOS with rights)
PING_HEADER_RE="^PING "
PING_REPLY_RE="bytes from"

is_parse_success_output() {
  local out="$1"

  # 1) unprivileged raw socket failure => parsing was OK
  if contains "$out" "$SOCKET_PERM_ERR"; then
    return 0
  fi

  # 2) privileged run => should print ping header and/or replies
  if print -- "$out" | grep -Eq "$PING_HEADER_RE"; then
    return 0
  fi
  if contains "$out" "$PING_REPLY_RE"; then
    return 0
  fi

  return 1
}

run_expect_parse_ok() {
  local desc="$1"; shift
  local out

  # Use -c 1 and a small timeout so privileged runs terminate quickly.
  out=$(run_cmd -c 1 -w 1 "$@" 127.0.0.1)

  if is_parse_success_output "$out"; then
    say "[OK]   $desc"
    ((pass++))
  else
    say "[FAIL] $desc"
    say "       cmd: $BIN -c 1 -w 1 $@ 127.0.0.1"
    say "       expected: either '$SOCKET_PERM_ERR' OR ping output"
    say "       out: ${out}"
    ((fail++))
  fi
}

run_expect_parse_fail() {
  local desc="$1"; shift
  local out

  out=$(run_cmd -c 1 -w 1 "$@" 127.0.0.1)

  if is_parse_success_output "$out"; then
    say "[FAIL] $desc"
    say "       cmd: $BIN -c 1 -w 1 $@ 127.0.0.1"
    say "       did not expect parse success"
    say "       out: ${out}"
    ((fail++))
  else
    say "[OK]   $desc"
    ((pass++))
  fi
}

say "== ft_ping arg parsing boundary checks =="
require_bin

# --- help / usage flags ---
# These should exit after printing usage.
out=$(run_cmd --help)
expect_contains "--help prints Options" "$out" "Options:"
expect_not_contains "--help must not ping" "$out" "$PING_REPLY_RE"

# Unknown option should print usage (current behavior).
out=$(run_cmd -x)
expect_contains "unknown option prints usage" "$out" "Usage: ft_ping"

# --- no-arg flags (should parse OK and attempt to run) ---
run_expect_parse_ok "-v (verbose)" -v
run_expect_parse_ok "--verbose" --verbose
run_expect_parse_ok "-q (quiet)" -q
run_expect_parse_ok "--quiet" --quiet

# --- destination handling ---
# No destination => parse error
out=$(run_cmd -c 1)
expect_contains "missing destination" "$out" "destination required"

# Multiple destinations => parse error
out=$(run_cmd 1.1.1.1 8.8.8.8)
expect_contains "multiple destinations" "$out" "multiple destinations provided"

# --- count (-c): min 1 ---
run_expect_parse_ok   "-c min (1)" -c 1
run_expect_parse_fail "-c below min (0)" -c 0
run_expect_parse_fail "-c negative" -c -1
run_expect_parse_fail "-c huge (overflow)" -c 999999999999999999999999

# --- ttl (--ttl): range 0..255 ---
run_expect_parse_ok   "--ttl min (0)" --ttl 0
run_expect_parse_ok   "--ttl max (255)" --ttl 255
run_expect_parse_fail "--ttl above max (256)" --ttl 256
run_expect_parse_fail "--ttl negative" --ttl -1
run_expect_parse_fail "--ttl huge (overflow)" --ttl 999999999999999999999999

# Also test attached long-opt argument style (--ttl99)
run_expect_parse_ok   "--ttl99 attached" --ttl99

# --- size (-s): min 0, max 65507 ---
run_expect_parse_ok   "-s min (0)" -s 0
run_expect_parse_ok   "-s max (65507)" -s 65507
run_expect_parse_fail "-s above max (65508)" -s 65508
run_expect_parse_fail "-s negative" -s -1
run_expect_parse_fail "-s huge (overflow)" -s 999999999999999999999999

# --- timeout (-w): allow >= 0 ---
run_expect_parse_ok   "-w zero" -w 0
run_expect_parse_ok   "-w small" -w 1
run_expect_parse_fail "-w negative" -w -1
run_expect_parse_fail "-w huge (overflow)" -w 999999999999999999999999

# --- interval (-i): allow 0, or >=0.002; reject short non-zero ---
run_expect_parse_ok   "-i zero" -i 0
run_expect_parse_ok   "-i min non-zero (0.002)" -i 0.002
run_expect_parse_fail "-i too short (0.001)" -i 0.001
run_expect_parse_fail "-i negative" -i -1
run_expect_parse_fail "-i junk" -i abc

# --- option requires argument (missing) ---
out=$(run_cmd -c)
expect_contains "-c missing arg" "$out" "option requires an argument"

out=$(run_cmd --ttl)
expect_contains "--ttl missing arg" "$out" "option requires an argument"

say "----------------------------------------"
say "Pass: $pass  Fail: $fail"

if (( fail > 0 )); then
  exit 1
fi
exit 0

