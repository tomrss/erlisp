#!/usr/bin/env bash
# usage: bench/profile.sh <test> [repeat]
# profiles bench/suite/<test>.erlisp with macOS sample(1), loading it
# <repeat> times (default 20) so the process lives long enough.
# build with -g for symbols, e.g. make CFLAGS="... -O2 -g ..."
set -euo pipefail

test=${1:?usage: $0 <test> [repeat]}
repeat=${2:-20}

root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/erlisp
src=$root/bench/suite/$test.erlisp
out=$root/bench/profiles
report=$out/$test.txt

[ -f "$src" ] || { echo "no such test: $src" >&2; exit 1; }
mkdir -p "$out"

driver=$(mktemp /tmp/erlisp-profile.XXXXXX)
trap 'rm -f "$driver"' EXIT
for _ in $(seq "$repeat"); do
  echo "(load \"$src\")"
done > "$driver"

"$bin" "$driver" > /dev/null &
pid=$!
sample "$pid" 600 1 -mayDie -file "$report" > /dev/null 2>&1 || true
wait "$pid" || true

echo "full report: $report"
awk '/Sort by top of stack/,0' "$report" | head -40
