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
    int repeats = 5;
    int warmups = 1;
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
      "  --repeats R      Timed runs per implementation (default 5, >=1)\n"
      "  --warmups W      Untimed runs per implementation (default 1, >=0)\n"
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
        else if (key == "--algorithm") o.algorithm = value;
        else if (key == "--input") o.input = value;
        else if (key == "--csv") o.csv = value;
        else throw std::invalid_argument("Unknown option: " + key);
    }
    if (o.points < 2 || o.threads < 1 || o.repeats < 1 || o.warmups < 0)
        throw std::invalid_argument("Require points >=2, threads >=1, repeats >=1, warmups >=0.");
    if (!o.input.empty() && (o.points_given || o.seed_given))
        throw std::invalid_argument("Use --input separately from --points/--seed.");
    if (o.algorithm != "all" && o.algorithm != "sequential" && o.algorithm != "openmp" &&
        o.algorithm != "tbb" && o.algorithm != "pstl")
        throw std::invalid_argument("Unknown algorithm: " + o.algorithm);
    return o;
}
using Kernel = cp::Result (*)(const cp::Problem&);
struct Stats { std::string name; cp::Result result; std::vector<double> samples; double median; };
Stats measure(const char* name, Kernel kernel, const cp::Problem& p, const Options& o,
              cp::Result expected) {
    for (int i = 0; i < o.warmups; ++i)
        if (!cp::same(kernel(p), expected)) throw std::runtime_error(std::string(name) + " failed warmup validation.");
    Stats s{name, {}, {}, 0};
    for (int i = 0; i < o.repeats; ++i) {
        const auto begin = std::chrono::steady_clock::now();
        const cp::Result result = kernel(p);
        const auto end = std::chrono::steady_clock::now();
        if (!cp::same(result, expected)) throw std::runtime_error(std::string(name) + " failed correctness validation.");
        s.result = result;
        s.samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
    }
    auto ordered = s.samples;
    std::sort(ordered.begin(), ordered.end());
    const auto m = ordered.size() / 2;
    s.median = ordered.size() % 2 ? ordered[m] : (ordered[m - 1] + ordered[m]) / 2;
    return s;
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
        stats.push_back(measure("sequential", cp::sequential, problem, o, expected));
        if (o.algorithm == "all" || o.algorithm == "openmp") stats.push_back(measure("openmp", cp::openmp, problem,o,expected));
        if (o.algorithm == "all" || o.algorithm == "tbb") stats.push_back(measure("tbb", cp::tbb, problem,o,expected));
        if (o.algorithm == "all" || o.algorithm == "pstl") stats.push_back(measure("pstl", cp::pstl, problem,o,expected));
        std::cout << "Compiler: " << __VERSION__ << "; Parallel STL: libstdc++/oneTBB\n"
                  << "Points: " << problem.points().size() << "; parallel worker limit: " << o.threads
                  << "; repeats: " << o.repeats << "; warmups: " << o.warmups << '\n';
        std::cout << std::setprecision(17) << "Closest pair (zero-based): (" << expected.i << ", " << expected.j
                  << "); distance: " << std::sqrt(expected.squared_distance) << '\n';
        const auto a = problem.points()[expected.i], b = problem.points()[expected.j];
        std::cout << "Coordinates: (" << a.x << ", " << a.y << ") and (" << b.x << ", " << b.y << ")\n";
        std::ofstream csv;
        if (!o.csv.empty()) {
            csv.open(o.csv);
            if (!csv) throw std::runtime_error("Cannot create CSV: " + o.csv);
            csv << "algorithm,points,worker_limit,repeats,warmups,source,seed,median_ms,min_ms,max_ms,speedup,efficiency,i,j,distance,compiler";
            for (int r=1; r<=o.repeats; ++r) csv << ",sample_" << r << "_ms";
            csv << '\n' << std::setprecision(17);
        }
        for (const auto& s : stats) {
            const int workers = s.name == "sequential" ? 1 : o.threads;
            const double speedup = s.median > 0 ? stats.front().median / s.median : 0;
            const auto range = std::minmax_element(s.samples.begin(), s.samples.end());
            std::cout << std::fixed << std::setprecision(3) << s.name << ": " << s.median
                      << " ms (min " << *range.first << ", max " << *range.second << "), speedup "
                      << speedup << "x; checked\n";
            if (csv.is_open()) {
                csv << s.name << ',' << problem.points().size() << ',' << workers << ',' << o.repeats
                    << ',' << o.warmups << ',' << csv_quote(o.input.empty() ? "generated" : o.input) << ',';
                if (o.input.empty()) csv << o.seed;
                csv << ',' << s.median << ',' << *range.first << ',' << *range.second << ',' << speedup
                    << ',' << speedup / workers << ',' << s.result.i << ',' << s.result.j << ','
                    << std::sqrt(s.result.squared_distance) << ',' << csv_quote(__VERSION__);
                for (double sample : s.samples) csv << ',' << sample;
                csv << '\n';
            }
        }
        if (csv.is_open()) { csv.flush(); if (!csv) throw std::runtime_error("Failed writing CSV."); }
        return 0;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
