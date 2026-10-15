#include "closest_pair.hpp"
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_reduce.h>
namespace cp {
Result tbb(const Problem& problem) {
    // oneTBB splits ranges into tasks and schedules/steals them among workers.
    return oneapi::tbb::parallel_reduce(
        oneapi::tbb::blocked_range<std::size_t>(0, problem.points().size() - 1, 8),
        Result{},
        [&](const oneapi::tbb::blocked_range<std::size_t>& range, Result best) {
            for (std::size_t i = range.begin(); i < range.end(); ++i)
                best = better(best, scan_row(problem, i));
            return best;
        },
        [](Result a, Result b) { return better(a, b); });
}
} // namespace cp
