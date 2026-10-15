#include "closest_pair.hpp"
#include <omp.h>
#include <oneapi/tbb/global_control.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
// Independent oracle: uses a direct nested loop without scan_row/better.
cp::Result oracle(const std::vector<cp::Point>& p) {
    cp::Result result;
    for (std::size_t i = 0; i < p.size(); ++i) for (std::size_t j = i+1; j < p.size(); ++j) {
        const double x = p[i].x-p[j].x, y = p[i].y-p[j].y;
        const double d = x*x+y*y;
        if (d < result.squared_distance) result = {d,i,j};
    }
    return result;
}
void check(const std::vector<cp::Point>& p) {
    const auto expected = oracle(p);
    cp::Problem problem(p);
    require(cp::same(cp::sequential(problem),expected),"Sequential disagrees with oracle");
    require(cp::same(cp::openmp(problem),expected),"OpenMP disagrees with oracle");
    require(cp::same(cp::tbb(problem),expected),"oneTBB disagrees with oracle");
    require(cp::same(cp::pstl(problem),expected),"Parallel STL disagrees with oracle");
}
void invalid(std::vector<cp::Point> p) {
    bool rejected = false;
    try { const cp::Problem problem(std::move(p)); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected,"Invalid input was accepted");
}
}
int main() {
    try {
        invalid({}); invalid({{0,0}});
        invalid({{0,0},{std::numeric_limits<double>::infinity(),0}});
        invalid({{0,0},{std::numeric_limits<double>::quiet_NaN(),0}});
        invalid({{0,0},{1e151,0}});
        for (int threads : {1,2,4}) {
            omp_set_dynamic(0); omp_set_num_threads(threads);
            const oneapi::tbb::global_control cap(oneapi::tbb::global_control::max_allowed_parallelism,threads);
            check({{0,0},{3,4}});
            require(cp::sequential(cp::Problem({{0,0},{3,4}})).squared_distance == 25,"Known 3-4-5 distance failed");
            check({{0,0},{1,0},{0,1},{1,1}}); // equal minima, lexicographic tie
            check({{7,7},{7,7},{7,7}}); // duplicate points
            check({{10,10},{100,100},{-5,-5},{-5,-5}}); // winner at last pair
            check({{-1e150,1e150},{1e150,-1e150}}); // large safe coordinates
            check({{0,0},{1e-160,0},{2e-160,0}}); // tiny distances
            std::vector<cp::Point> line;
            for(int i=0;i<65;++i) line.push_back({double(i),0});
            check(line);
            for (std::uint64_t seed=0;seed<15;++seed)
                for (std::size_t n : {2,3,7,32,127,256}) check(cp::generate_points(n,seed));
        }
        std::cout << "All four implementations passed oracle tests at 1, 2, and 4 workers.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
