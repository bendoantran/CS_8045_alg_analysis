#include "closest_pair.hpp"
#include <omp.h>
#include <oneapi/tbb/global_control.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Options {
    std::size_t points = 10000;
    std::uint64_t seed = 42;
    int threads = omp_get_max_threads();
    int repeats = 9;
    int warmups = 3;
    int min_sample_ms = 25;
    std::string algorithm = CP_DEFAULT_ALGORITHM;
    std::string input, csv;
    bool points_given = false, seed_given = false;
};
template<class T> T integer(const std::string& text, const std::string& option) {
    T value{};
    auto r = std::from_chars(text.data(), text.data() + text.size(), value);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid integer for " + option + ": " + text);
    return value;
}
void help() {
    std::cout << "Brute-force 2D closest pair (C++17)\n"
      "  --algorithm all|sequential|openmp|tbb|pstl\n"
      "  --points N       Generated point count (default 10000, >=2)\n"
      "  --seed S         Generation seed (default 42)\n"
      "  --input FILE     Read x,y or x y lines; no header; # comments allowed\n"
      "  --threads T      Parallel worker limit (default OpenMP environment/CPU count)\n"
      "  --repeats R      Timed runs per implementation (default 9, >=1)\n"
      "  --warmups W      Untimed runs per implementation (default 3, >=0)\n"
      "  --min-sample-ms M Target batch duration (default 25; 0 disables batching)\n"
      "  --csv FILE       Write summary with every timed sample\n"
      "  --help           Show this text\n";
}
Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help") { help(); std::exit(0); }
        if (i + 1 == argc) throw std::invalid_argument("Missing value for " + key);
        std::string value = argv[++i];
        if (key == "--points") { o.points = integer<std::size_t>(value,key); o.points_given = true; }
        else if (key == "--seed") { o.seed = integer<std::uint64_t>(value,key); o.seed_given = true; }
        else if (key == "--threads") o.threads = integer<int>(value,key);
        else if (key == "--repeats") o.repeats = integer<int>(value,key);
        else if (key == "--warmups") o.warmups = integer<int>(value,key);
        else if (key == "--min-sample-ms") o.min_sample_ms = integer<int>(value,key);
        else if (key == "--algorithm") o.algorithm = value;
        else if (key == "--input") o.input = value;
        else if (key == "--csv") o.csv = value;
        else throw std::invalid_argument("Unknown option: " + key);
    }
    if (o.points < 2 || o.threads < 1 || o.repeats < 1 || o.warmups < 0 || o.min_sample_ms < 0 || o.min_sample_ms > 60000)
        throw std::invalid_argument("Require points >=2, threads >=1, repeats >=1, warmups >=0, and min-sample-ms in [0,60000].");
    if (!o.input.empty() && (o.points_given || o.seed_given))
        throw std::invalid_argument("Use --input separately from --points/--seed.");
    if (o.algorithm != "all" && o.algorithm != "sequential" && o.algorithm != "openmp" &&
        o.algorithm != "tbb" && o.algorithm != "pstl")
        throw std::invalid_argument("Unknown algorithm: " + o.algorithm);
    return o;
}
using Kernel = cp::Result (*)(const cp::Problem&);
struct Stats {
    std::string name;
    Kernel kernel;
    cp::Result result{};
    std::vector<double> samples;
    double median = 0;
    std::size_t iterations = 1;
};
// Every call is checked and used. Batch validation overhead is included equally
// for all kernels; there is no per-call clock read inside the measured batch.
double run_batch(Stats& s, const cp::Problem& p, cp::Result expected) {
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t call = 0; call < s.iterations; ++call) {
        const auto result = s.kernel(p);
        if (!cp::same(result, expected))
            throw std::runtime_error(s.name + " failed correctness validation.");
        s.result = result;
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
void measure(std::vector<Stats>& stats, const cp::Problem& p, const Options& o,
             cp::Result expected) {
    for (int pass = 0; pass < o.warmups; ++pass)
        for (std::size_t offset = 0; offset < stats.size(); ++offset)
            run_batch(stats[(static_cast<std::size_t>(pass) + offset) % stats.size()], p, expected);
    // Untimed calibration grows batches until they reach the target duration.
    // Calibration is never included in the samples used to calculate speedup.
    constexpr std::size_t max_iterations = std::size_t{1} << 24;
    for (auto& s : stats) {
        if (o.min_sample_ms == 0) continue;
        while (run_batch(s, p, expected) < o.min_sample_ms) {
            if (s.iterations >= max_iterations) break;
            s.iterations *= 2;
        }
    }
    // Interleave samples and rotate which implementation runs first. All four
    // methods now encounter machine drift throughout the same measurement period.
    for (int round = 0; round < o.repeats; ++round) {
        for (std::size_t offset = 0; offset < stats.size(); ++offset) {
            auto& s = stats[(static_cast<std::size_t>(round) + offset) % stats.size()];
            const double elapsed = run_batch(s, p, expected);
            s.samples.push_back(elapsed / static_cast<double>(s.iterations));
        }
    }
    for (auto& s : stats) {
        auto ordered = s.samples;
        std::sort(ordered.begin(), ordered.end());
        const auto m = ordered.size() / 2;
        s.median = ordered.size() % 2 ? ordered[m] : (ordered[m - 1] + ordered[m]) / 2;
    }
}
std::string csv_quote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) { if (c == '"') out += '"'; out += c; }
    return out + '"';
}
}
int main(int argc, char** argv) {
    try {
        const Options o = parse(argc, argv);
        const cp::Problem problem(o.input.empty() ? cp::generate_points(o.points,o.seed) : cp::read_points(o.input));
        omp_set_dynamic(0);
        omp_set_num_threads(o.threads);
        const oneapi::tbb::global_control cap(oneapi::tbb::global_control::max_allowed_parallelism,
                                             static_cast<std::size_t>(o.threads));
        const cp::Result expected = cp::sequential(problem);
        // A sequential baseline is always measured, including single-backend runs.
        std::vector<Stats> stats;
        stats.push_back(Stats{"sequential", cp::sequential});
        if (o.algorithm == "all" || o.algorithm == "openmp") stats.push_back(Stats{"openmp", cp::openmp});
        if (o.algorithm == "all" || o.algorithm == "tbb") stats.push_back(Stats{"tbb", cp::tbb});
        if (o.algorithm == "all" || o.algorithm == "pstl") stats.push_back(Stats{"pstl", cp::pstl});
        measure(stats, problem, o, expected);
        std::cout << "Compiler: " << __VERSION__ << "; Parallel STL: libstdc++/oneTBB\n"
                  << "Points: " << problem.points().size() << "; parallel worker limit: " << o.threads
                  << "; repeats: " << o.repeats << "; warmups: " << o.warmups << '\n'
                  << "Timing: rotating interleaved batches; target sample duration: " << o.min_sample_ms << " ms\n";
        std::cout << std::setprecision(17) << "Closest pair (zero-based): (" << expected.i << ", " << expected.j
                  << "); distance: " << std::sqrt(expected.squared_distance) << '\n';
        const auto a = problem.points()[expected.i], b = problem.points()[expected.j];
        std::cout << "Coordinates: (" << a.x << ", " << a.y << ") and (" << b.x << ", " << b.y << ")\n";
        std::ofstream csv;
        if (!o.csv.empty()) {
            csv.open(o.csv);
            if (!csv) throw std::runtime_error("Cannot create CSV: " + o.csv);
            csv << "algorithm,points,worker_limit,repeats,warmups,source,seed,median_ms,min_ms,max_ms,speedup,efficiency,i,j,distance,compiler,timing_protocol,sample_iterations,min_sample_ms";
            for (int r=1; r<=o.repeats; ++r) csv << ",sample_" << r << "_ms";
            csv << '\n' << std::setprecision(17);
        }
        for (const auto& s : stats) {
            const int workers = s.name == "sequential" ? 1 : o.threads;
            const double speedup = s.median > 0 ? stats.front().median / s.median : 0;
            const auto range = std::minmax_element(s.samples.begin(), s.samples.end());
            std::cout << std::fixed << std::setprecision(3) << s.name << ": " << s.median
                      << " ms (min " << *range.first << ", max " << *range.second << "), speedup "
                      << speedup << "x; efficiency " << 100 * speedup / workers
                      << "%; " << s.iterations << " calls/sample; checked\n";
            if (csv.is_open()) {
                csv << s.name << ',' << problem.points().size() << ',' << workers << ',' << o.repeats
                    << ',' << o.warmups << ',' << csv_quote(o.input.empty() ? "generated" : o.input) << ',';
                if (o.input.empty()) csv << o.seed;
                csv << ',' << s.median << ',' << *range.first << ',' << *range.second << ',' << speedup
                    << ',' << speedup / workers << ',' << s.result.i << ',' << s.result.j << ','
                    << std::sqrt(s.result.squared_distance) << ',' << csv_quote(__VERSION__)
                    << ",interleaved_batched_v2," << s.iterations << ',' << o.min_sample_ms;
                for (double sample : s.samples) csv << ',' << sample;
                csv << '\n';
            }
        }
        if (csv.is_open()) { csv.flush(); if (!csv) throw std::runtime_error("Failed writing CSV."); }
        return 0;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
