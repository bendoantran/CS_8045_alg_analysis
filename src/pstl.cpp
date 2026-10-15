#include "closest_pair.hpp"
#include <execution>
#include <numeric>
#if defined(CP_REQUIRE_TBB_PSTL) && !defined(_PSTL_PAR_BACKEND_TBB)
#error "Parallel STL is not using the expected libstdc++ TBB backend. See README.md."
#endif
namespace cp {
Result pstl(const Problem& problem) {
    // Transform each row into its minimum, then reduce those minima with par.
    // Callbacks only read the input and return values: no shared mutable result.
    const auto& rows = problem.rows();
    return std::transform_reduce(std::execution::par, rows.begin(), rows.end(),
        Result{}, [](Result a, Result b) { return better(a, b); },
        [&](std::size_t i) { return scan_row(problem, i); });
}
} // namespace cp
