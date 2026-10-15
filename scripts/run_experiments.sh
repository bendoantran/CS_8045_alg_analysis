#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -x "$project_dir/build/benchmark" ]]; then echo "Run scripts/setup.sh first." >&2; exit 1; fi
# Override, e.g. POINT_COUNTS="1000 10000" THREAD_COUNTS="1 2 4" REPEATS=7 bash scripts/run_experiments.sh
read -r -a sizes <<< "${POINT_COUNTS:-1000 5000 10000 20000}"
read -r -a workers <<< "${THREAD_COUNTS:-1 2 4 8}"
run_dir="$project_dir/results/$(date +%Y%m%d_%H%M%S)_$$"
mkdir -p "$run_dir"
{
  uname -a
  "${CXX:-g++}" --version
  cmake --version
  if command -v lscpu >/dev/null; then lscpu; fi
  if command -v sysctl >/dev/null && [[ $(uname -s) == Darwin ]]; then sysctl -n machdep.cpu.brand_string hw.ncpu; fi
  printf 'POINT_COUNTS=%s\nTHREAD_COUNTS=%s\nREPEATS=%s\nWARMUPS=%s\n' "${sizes[*]}" "${workers[*]}" "${REPEATS:-5}" "${WARMUPS:-1}"
  cat "$project_dir/build/CMakeCache.txt"
} > "$run_dir/environment.txt"
for n in "${sizes[@]}"; do
  for t in "${workers[@]}"; do
    "$project_dir/build/benchmark" --points "$n" --threads "$t" --seed 42 \
      --repeats "${REPEATS:-5}" --warmups "${WARMUPS:-1}" --csv "$run_dir/n${n}_t${t}.csv" \
      | tee "$run_dir/n${n}_t${t}.txt"
  done
done
printf '\nResults saved to %s\n' "$run_dir"
