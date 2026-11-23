#!/usr/bin/env bash
# Sweeps a grid of (k, load factor) settings over every contender that supports the setting and
# extracts Pareto fronts. Contenders that cannot run a setting skip it on their own.
#
#   ./scripts/pareto.sh [build-dir] [-- extra Comparison flags]
#   K="1,2,4,8,16" ALPHA="0.8,0.9,0.95,1" N=5000000 ./scripts/pareto.sh build -- --staticSet
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:-build}"; shift || true
[ "${1:-}" = "--" ] && shift
K="${K:-1,2,4,8,16}"
ALPHA="${ALPHA:-0.8,0.9,0.95,1}"
N="${N:-5000000}"
out="${OUT:-pareto-results}"
mkdir -p "$out"
"$build/Comparison" --all --numKeys "$N" --k "$K" --loadFactor "$ALPHA" "$@" | tee "$out/results.log"
python3 "$here/pareto.py" "$out/results.log" --csv "$out/fronts.csv" --plot "$out/plots" | tee "$out/fronts.txt"
if grep -q "^SETRESULT" "$out/results.log"; then
    python3 "$here/pareto.py" "$out/results.log" --set --csv "$out/set_fronts.csv" --plot "$out/set_plots" \
        | tee "$out/set_fronts.txt"
fi
