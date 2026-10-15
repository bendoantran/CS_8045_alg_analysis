#include "closest_pair.hpp"
#include <algorithm>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cp {
Problem::Problem(std::vector<Point> points) : points_(std::move(points)) {
    if (points_.size() < 2) throw std::invalid_argument("At least two points are required.");
    if (points_.size() > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
        throw std::invalid_argument("Too many points for the loop index type.");
    for (auto p : points_) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            std::abs(p.x) > 1e150 || std::abs(p.y) > 1e150)
            throw std::invalid_argument("Coordinates must be finite and have magnitude <= 1e150.");
    }
    rows_.resize(points_.size() - 1);
    std::iota(rows_.begin(), rows_.end(), std::size_t{0});
}
std::vector<Point> generate_points(std::size_t count, std::uint64_t seed) {
    std::mt19937_64 engine(seed);
    // Fixed bit-to-double mapping gives reproducible data across standard libraries.
    auto coordinate = [&] { return (static_cast<double>(engine() >> 11) * 0x1.0p-53 - 0.5) * 2e6; };
    std::vector<Point> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) points.push_back({coordinate(), coordinate()});
    return points;
}
std::vector<Point> read_points(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("Cannot open input file: " + filename);
    std::vector<Point> points;
    std::string line;
    std::size_t number = 0;
    while (std::getline(file, line)) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        row >> std::ws;
        if (row.eof()) continue;
        Point p{};
        std::string extra;
        if (!(row >> p.x >> p.y) || (row >> extra))
            throw std::runtime_error("Invalid point at line " + std::to_string(number) + "; expected x,y.");
        points.push_back(p);
    }
    if (file.bad()) throw std::runtime_error("Failed while reading input file.");
    return points;
}
} // namespace cp
