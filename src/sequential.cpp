#include "closest_pair.hpp"
namespace cp {
Result sequential(const Problem& problem) {
    Result best;
    for (std::size_t i = 0; i + 1 < problem.points().size(); ++i)
        best = better(best, scan_row(problem, i));
    return best;
}
} // namespace cp
