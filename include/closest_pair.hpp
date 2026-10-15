#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cp {
struct Point { double x; double y; };
struct Result {
    double squared_distance = std::numeric_limits<double>::infinity();
    std::size_t i = std::numeric_limits<std::size_t>::max();
    std::size_t j = std::numeric_limits<std::size_t>::max();
};
// A total order makes parallel reduction associative and deterministic.
inline Result better(Result a, Result b) noexcept {
    if (b.squared_distance < a.squared_distance ||
        (b.squared_distance == a.squared_distance &&
         (b.i < a.i || (b.i == a.i && b.j < a.j)))) return b;
    return a;
}
inline bool same(Result a, Result b) noexcept {
    return a.squared_distance == b.squared_distance && a.i == b.i && a.j == b.j;
}
// Validation and allocation happen once, before any kernel timing.
class Problem {
public:
    explicit Problem(std::vector<Point> points);
    const std::vector<Point>& points() const noexcept { return points_; }
    const std::vector<std::size_t>& rows() const noexcept { return rows_; }
private:
    std::vector<Point> points_;
    std::vector<std::size_t> rows_;
};
inline Result scan_row(const Problem& problem, std::size_t i) noexcept {
    const auto& p = problem.points();
    Result best;
    for (std::size_t j = i + 1; j < p.size(); ++j) {
        const double dx = p[i].x - p[j].x;
        const double dy = p[i].y - p[j].y;
        best = better(best, Result{dx * dx + dy * dy, i, j});
    }
    return best;
}
Result sequential(const Problem&);
Result openmp(const Problem&);
Result tbb(const Problem&);
Result pstl(const Problem&);
std::vector<Point> generate_points(std::size_t count, std::uint64_t seed);
std::vector<Point> read_points(const std::string& filename);
} // namespace cp
