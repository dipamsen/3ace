#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

make >/dev/null

run_case() {
  name="$1"

  src="tests/${name}.src"
  expected="tests/${name}.out"

  ./a.out "$src" > /tmp/${name}.actual
  diff -u "$expected" /tmp/${name}.actual
  echo "PASS: $name"
}

run_case 01_expression
run_case 02_control_flow
run_case 03_bool
run_case 04_break_continue
run_case 05_loops_arrays
