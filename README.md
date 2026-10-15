# Brute-force closest pair: sequential, OpenMP, oneTBB, Parallel STL

A C++17 project comparing four implementations of the **same O(n²) algorithm** for the two-dimensional Euclidean closest pair problem. This project includes source code, a CMake build, independent correctness tests, repeatable benchmarks, CSV export, and scripts for setup and experiments.

## Quick start on the computer where this project was created

```bash
cd /home/ldoan4/Documents/CS_8045_alg_analysis
bash scripts/setup.sh --fetch-tbb
./build/benchmark --points 10000 --threads 4 --repeats 5 --csv results/example.csv
```

`--fetch-tbb` builds oneTBB inside `build/`; it does not require administrator access. The supplied `oneTBB-v2021.13.0.tar.gz` is used when present, so oneTBB itself does not need to be downloaded again. The archive is verified with SHA-256:

```text
3ad5dd08954b39d113dc5b3f8a8dc6dc1fd5250032b7c491eb07aed5c94133e1
```

If the archive is absent, CMake downloads exactly the same release from the official oneTBB repository. Internet access is then required during configuration. Compilers, CMake, and a build tool must already be installed; instructions follow.

## Setup on another computer

Copy the source project, including its oneTBB archive, to the other computer. **Do not copy/reuse `build/`**: build directories, runtime paths, and binaries belong to the machine and compiler that created them. Results may be copied separately. Choose a local project directory and run all commands from that directory.

The supported toolchain is **GCC/g++ with libstdc++, C++17, CMake >=3.20, OpenMP, and oneTBB >=2021**. GCC 11 or newer is recommended. CMake deliberately rejects other compilers so the Parallel STL backend and worker limits stay consistent. Native MSVC and Apple Clang are outside this project's supported configuration.

### Ubuntu / Debian / Windows WSL2 (recommended)

Install the build tools and oneTBB:

```bash
sudo apt update
sudo apt install -y build-essential cmake libtbb-dev
bash scripts/setup.sh
```

Or install just the compiler/build tools and use the included oneTBB source:

```bash
sudo apt update
sudo apt install -y build-essential cmake
bash scripts/setup.sh --fetch-tbb
```

If an older Linux distribution provides CMake <3.20 or an old TBB release, install a supported CMake/GCC or use a newer distribution. `--fetch-tbb` supplies a compatible oneTBB, not a compiler.

For Windows, install WSL2 with an Ubuntu distribution (`wsl --install` from an administrator PowerShell, if WSL is not installed), open Ubuntu, and follow the Linux commands above. Keep the project in the WSL Linux filesystem, for example `~/CS_8045_alg_analysis`, for better build performance. C++ code and scripts run inside WSL.

### Fedora

```bash
sudo dnf install -y gcc-c++ cmake make tbb-devel
bash scripts/setup.sh
```

Alternatively omit `tbb-devel` and run `bash scripts/setup.sh --fetch-tbb`.

### macOS using Homebrew GCC

Install Apple's command-line build tools and Homebrew (if absent), then:

```bash
xcode-select --install  # only if command-line tools are missing
brew install gcc cmake
ls "$(brew --prefix gcc)/bin/"g++-*
```

Set `CXX` to the **versioned GCC executable printed by the last command**, for example `/opt/homebrew/opt/gcc/bin/g++-15` if that is the executable installed on your computer:

```bash
export CXX=/actual/path/to/g++-VERSION
bash scripts/setup.sh --fetch-tbb
./build/benchmark --points 10000 --threads 4
```

The example compiler version is illustrative; use your installed version. Apple's `/usr/bin/g++` is Clang and is rejected. Homebrew GCC provides its own OpenMP runtime; the locally built oneTBB avoids mixing compiler installations. macOS instructions have not been tested on the original Linux computer.

### Manual CMake build (works on supported toolchains)

For system-installed oneTBB:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

To build the pinned local oneTBB, add `-DCP_FETCH_TBB=ON` to the configure command. For a oneTBB installation in a custom prefix, use `-DCMAKE_PREFIX_PATH=/path/to/tbb/prefix`. No global `LD_LIBRARY_PATH` change is needed for the supplied Linux build: CMake sets build-time runtime-library paths automatically.

To change compilers or move to a different computer, configure a **new build directory**, e.g. `cmake -S . -B build-new ...`. The setup script always uses `build/`. `BUILD_JOBS=2 bash scripts/setup.sh --fetch-tbb` limits build concurrency on a machine with little memory; the default is four build jobs.

## Source layout and implementations

- `include/closest_pair.hpp`: point/result types, deterministic minimum operation, shared distance calculation, and public kernel declarations.
- `src/common.cpp`: dataset validation, deterministic random generation, and file parsing.
- `src/sequential.cpp`: single-threaded nested-loop baseline.
- `src/openmp.cpp`: `#pragma omp parallel for`, dynamic row scheduling, and a custom reduction over private closest-pair results.
- `src/tbb.cpp`: oneTBB `parallel_reduce` over a `blocked_range`, with task scheduling and partial-result reduction.
- `src/pstl.cpp`: `std::transform_reduce(std::execution::par, ...)` over row indices.
- `src/main.cpp`: shared command-line interface, benchmark timing, correctness validation, and CSV reporting.
- `tests/correctness.cpp`: independent brute-force oracle and edge cases.
- `scripts/setup.sh`: configure, compile, and run CTest.
- `scripts/run_experiments.sh`: run multiple dataset sizes/worker limits and save machine information, raw samples, and summaries.
- `data/example.csv`: sample user-provided dataset.
- `results/`: generated benchmark output.

For each point `i`, every implementation scans `j = i+1 ... n-1`. Thus each unordered pair is evaluated exactly once: **n(n-1)/2 distance evaluations**. No version exits early when it finds distance zero. All use squared Euclidean distance inside the loop and take the square root only when reporting the final answer.

OpenMP uses chunks of eight rows with dynamic scheduling because early rows have more work. oneTBB uses a grain size of eight rows and its default automatic partitioner. Parallel STL delegates partitioning to libstdc++. Scheduling and reduction overheads therefore differ while the distance kernel remains the same.

**Parallel STL uses oneTBB underneath on this GCC/libstdc++ build.** The comparison is between different programming interfaces/scheduling paths, not four independent runtimes. A compile-time check refuses libstdc++'s serial PSTL fallback. `std::execution::par` allows parallel execution, but the implementation can still process small workloads serially. The explicit oneTBB and Parallel STL calls share a `global_control` worker cap set before timing. The C++ standard itself has no portable thread-count parameter; this cap is specific to this backend.

## Run individual versions

Each executable selects the named implementation by default:

```bash
./build/closest_sequential --points 10000 --repeats 5
./build/closest_openmp --points 10000 --threads 4 --repeats 5
./build/closest_tbb --points 10000 --threads 4 --repeats 5
./build/closest_pstl --points 10000 --threads 4 --repeats 5
```

Every run also measures the sequential baseline to validate results and calculate speedup. The sequential executable remains single-threaded in its computation even though the shared build links the other libraries. All executables accept the same arguments; `--algorithm` can override their default.

Run all four on the same data:

```bash
./build/benchmark --points 20000 --seed 42 --threads 8 --warmups 2 --repeats 7 \
  --csv results/n20000_t8.csv
./build/benchmark --help
```

Defaults: 10,000 points, seed 42, three warmups, nine timed batches, and the worker count reported by the OpenMP runtime (which respects the OpenMP environment when `--threads` is absent). `--threads` must be >=1 and explicitly sets the OpenMP team request and oneTBB worker cap. The cap is **not a measurement of how many workers actually ran**; runtimes may use fewer.

## Use your own points

```bash
./build/benchmark --input data/example.csv --threads 4 --csv results/custom.csv
```

Input is one point per line: `x,y` or `x y`. There is **no header row**. Blank lines and `#` comments are allowed. Indices refer to the zero-based sequence of parsed points, not physical file line numbers. At least two points are required. Duplicate points are allowed and produce distance zero.

Coordinates must be finite doubles with magnitude <=1e150, preventing overflow in squared distances. At very small scales, double precision can underflow or round distinct distances to the same value. Results represent the minimum under that floating-point arithmetic. Exact ties use the lexicographically smallest `(i,j)` pair, consistently across all versions. The build disables fused multiply-add contraction to keep the common distance formula consistent and does not enable `-ffast-math`.

`--input` cannot be combined with `--points` or `--seed`. Generated coordinates lie in [-1,000,000, 1,000,000), using `mt19937_64` and an explicit bit-to-double conversion for reproducible datasets.

## Benchmark methodology and comparison

The driver uses `std::chrono::steady_clock` and rotates implementation order between measurement rounds. It generates/loads and validates the data, prepares row indices, and configures worker caps **outside** the timed interval. Each measured interval includes a calibrated batch of full kernel calls, their task scheduling, reductions, and an O(1) result check after each call. Calibration targets at least 25 ms per batch before measurement; `--min-sample-ms` controls that target. The duration is divided by the number of calls to obtain a per-call sample. Calibration and warmups are discarded. Runtime initialization and CPU scheduling/frequency changes can affect first runs; warmups and longer interleaved samples reduce that effect. Each warmup and measured result must exactly match an independently obtained sequential result, including indices.

Output reports the median, minimum, and maximum of the batch-averaged per-call times in milliseconds, speedup, and efficiency. CSV records `timing_protocol`, `sample_iterations`, and `min_sample_ms` so the measurement method is explicit. CSV also stores every timed sample, seed/source, compiler version, worker limit, closest-pair indices/distance, and efficiency. Formulas:

```text
speedup = sequential median / parallel median
efficiency (CSV fraction) = speedup / requested worker limit
efficiency (%) = 100 * speedup / requested worker limit
```

Efficiency uses the requested cap, not an observed worker count. It is not CPU utilization or a percentage physically bounded by 100%. Measured efficiency can exceed 100% if speedup exceeds the worker count (superlinear speedup), due to cache effects, differences in generated code, or measurement variability. A one-thread backend can also run slightly faster than the sequential implementation without gaining anything from parallelism. Values are never clamped; investigate large excursions and sample variability. The raw benchmark CSV metrics compare each backend to the separate sequential implementation. The table report now defaults to same-backend strong scaling, comparing that backend at one worker with itself at p workers; the original sequential comparison remains selectable. Speedup below one means the parallel version was slower in that run.

Measurement rounds interleave the selected methods and rotate which one runs first. Earlier saved runs without a `timing_protocol` field used five short, separate samples with the sequential method first; those historical results can contain inflated speedup and efficiency from timing drift. Keep them as historical measurements rather than treating them as the corrected protocol. Repeat whole benchmark processes, use sufficiently large data, and report variability. Run on an otherwise idle machine, use Release builds, document hardware/compiler/library versions, and avoid worker limits exceeding available cores unless studying oversubscription. Small workloads often show little speedup because scheduling dominates.

**All four algorithms remain O(n²) time**; parallelism distributes work across cores. Input storage and the shared row-index vector use O(n) memory; thread/task reductions add runtime-dependent temporary storage. The sequential kernel itself uses O(1) extra space. Doubling n approximately quadruples the pair count. Start small before using very large n.

Run the supplied experiment sweep:

```bash
bash scripts/run_experiments.sh
# Custom sweep:
POINT_COUNTS="1000 5000 10000 20000" THREAD_COUNTS="1 2 4 8" REPEATS=7 WARMUPS=2 \
  bash scripts/run_experiments.sh
```

Each run creates a unique `results/<timestamp>_<pid>/` directory containing one CSV and text log per configuration plus `environment.txt` with OS, compiler, CPU information when available, experiment parameters, and the CMake cache. CSV files can be opened in Excel, LibreOffice, or Python. No additional Python packages are needed to build or run this project.

## Tests and troubleshooting

```bash
ctest --test-dir build --output-on-failure
./build/correctness_tests
```

Tests compare all four kernels against a separate direct-loop oracle at worker limits 1, 2, and 4. Cases include known distances, ties, duplicates, a winning last pair, collinear points, tiny/large coordinates, randomized datasets, and invalid input. CTest additionally exercises the command-line interface and file input. A failing test exits with a nonzero status.

- **TBB package not found:** install `libtbb-dev`/`tbb-devel`, supply `CMAKE_PREFIX_PATH`, or use `bash scripts/setup.sh --fetch-tbb`.
- **OpenMP missing:** use the supported GCC installation. On Linux, install the distribution's full GCC development toolchain. On macOS, set `CXX` to Homebrew's versioned GCC executable.
- **PSTL backend check failed:** make sure modern oneTBB headers are on the compiler include path and use GCC/libstdc++. CMake propagates `TBB::tbb` include/link requirements to every executable.
- **Compiler changed in an old cache:** configure a new build directory rather than reusing a cache made by another compiler.
- **oneTBB download failed:** copy the supplied archive alongside `CMakeLists.txt`, or install oneTBB through the OS package manager.
- **Cannot create CSV:** choose a writable path whose parent directory already exists (`results/` is included).
- **No speedup / too many workers:** reduce `--threads`, increase dataset size carefully, check for competing CPU workloads, and repeat measurements.

## Verification on the original computer

Verified on October 14, 2026 with GCC 13.3.0, CMake 3.28.3, OpenMP 4.5, and locally built oneTBB 2021.13.0 on Linux. All five CTest tests passed, and all four individual executables ran successfully. The experiment script completed all 12 configurations (1,000 / 5,000 / 10,000 points at worker limits 1 / 2 / 4 / 8). Every implementation produced the same indices and distance; the saved CSVs contain five timed samples per implementation. These checks verify this Linux environment; other operating-system setup instructions still need to be exercised on their respective machines.

## Primary references

- [CMake OpenMP imported targets](https://cmake.org/cmake/help/latest/module/FindOpenMP.html)
- [oneTBB parallel_reduce API](https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/2021-6/parallel-reduce.html)
- [GCC libstdc++ linking requirements for execution policies](https://gcc.gnu.org/onlinedocs/libstdc++/manual/using.html)
- [Official pinned oneTBB v2021.13.0 source](https://github.com/oneapi-src/oneTBB/tree/v2021.13.0)

The included third-party oneTBB source retains its upstream license files (Apache-2.0 and associated notices) inside the archive.

## Generate the three result tables

After running experiments, generate an interactive HTML report with exactly three tables: median time (milliseconds), efficiency (percent), and speedup. Point counts are columns and requested thread counts are rows. Select the benchmark run and implementation in the report.

```bash
python3 scripts/make_tables.py
# Or select one result folder:
python3 scripts/make_tables.py results/20261014_202212_125012
```

Open `results/benchmark_tables.html` in a browser. This uses only Python's standard library and embeds all table data in the report, so it also works offline. Regenerate it after a new experiment run. Efficiency uses the sequential baseline from the same configuration and the requested worker limit.

### Timing correction

The large >100% efficiencies in the original 1,000-point data were traced to unstable short sequential-first samples. For example, run `20261014_204015_132428/n1000_t1.csv` records a 0.743694 ms sequential median and 0.375090 ms OpenMP median, yielding 198.27% at one requested worker. The sequential samples themselves ranged down to 0.376417 ms. This is evidence of unstable timing; it does not demonstrate a benefit from multiple threads. The exact OS/CPU cause was not isolated.

The current driver uses interleaved measurement rounds and calibrated batches, and the experiment defaults are nine samples, three warmups, and a 25 ms calibration target. Run a fresh sweep and regenerate the tables:

```bash
bash scripts/setup.sh --fetch-tbb
REPEATS=9 WARMUPS=3 MIN_SAMPLE_MS=25 bash scripts/run_experiments.sh
python3 scripts/make_tables.py
```

References: [Cornell: speedup and efficiency](https://www.cs.cornell.edu/courses/cs5220/2020fa/lec/2020-09-08-perf-basics.html), [Cornell: superlinear speedup and cache effects](https://cvw.cac.cornell.edu/scalability/scalability-speedup/faster-or-more), and [Google Benchmark: minimum runtime and warmup](https://github.com/google/benchmark/blob/main/docs/user_guide.md).

The corrected sweep is saved in `results/20261014_210045_142461/`: all 16 configurations passed result comparisons, and all six CTest tests passed, including CSV checks for batching metadata, even/odd medians, and the unmodified speedup/efficiency formulas. At 1,000 points and one requested worker, the fresh OpenMP result was 0.367941 ms against a 0.367826 ms sequential median (99.97% efficiency), compared with 198.27% in the unstable old run. These are observed values for this run, not guarantees about future timings.

### Two distinct baselines in the tables

The report's **Metric baseline** selector separates two questions:

- **Same implementation at 1 thread** (default): how does adding threads help OpenMP, oneTBB, or Parallel STL individually? For implementation A, `scaling_speedup = T_A(1) / T_A(p)` and `scaling_efficiency (%) = 100 * T_A(1) / (p * T_A(p))`. The one-thread row is exactly 1× and 100% by definition. This requires matching datasets and one-thread results in the selected run.
- **Sequential C++ reference**: how fast is each implementation compared with the dedicated sequential C++ baseline? This retains the original CSV formulas. One-thread results may exceed 100% if a backend measures slightly faster than that separate baseline.

Changing the baseline changes the interpretation of speedup and efficiency. It does not change the recorded runtimes, clamp results, or correct the input data to produce a desired percentage. Strong-scaling efficiency can also exceed 100% in other datasets due to superlinear effects or measurement variability.

For the saved corrected run `20261014_210045_142461`, the four sequential-comparison efficiencies above 100% are all at one requested thread (100.08%–100.92%). Using the same implementation's one-thread measurement as its strong-scaling baseline yields 100% at one thread and below 100% at every multi-thread configuration in this particular run. The raw CSV columns `speedup` and `efficiency` retain their original sequential-reference definition; the report derives additional `scaling_speedup` and `scaling_efficiency` values from the measured times.
