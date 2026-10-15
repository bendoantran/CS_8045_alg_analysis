#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
fetch=OFF
if [[ ${1:-} == --fetch-tbb ]]; then fetch=ON; shift; fi
if (( $# > 0 )); then echo "Usage: bash scripts/setup.sh [--fetch-tbb]" >&2; exit 1; fi
for tool in cmake "${CXX:-g++}"; do
  if ! command -v "$tool" >/dev/null; then echo "Missing $tool; follow README.md." >&2; exit 1; fi
done
build_dir="$project_dir/build"
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DCP_FETCH_TBB="$fetch" -DCMAKE_CXX_COMPILER="${CXX:-g++}"
cmake --build "$build_dir" --parallel "${BUILD_JOBS:-4}"
ctest --test-dir "$build_dir" --output-on-failure
printf '\nReady. Run: "%s/benchmark" --points 10000 --threads 4\n' "$build_dir"
