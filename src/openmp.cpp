#include "closest_pair.hpp"
// OpenMP gives each worker a private Result and combines those results afterward.
#pragma omp declare reduction(cp_min : cp::Result : omp_out = cp::better(omp_out, omp_in)) initializer(omp_priv = cp::Result{})
namespace cp {
Result openmp(const Problem& problem) {
    Result best;
    const auto rows = static_cast<std::ptrdiff_t>(problem.points().size() - 1);
    // Early rows contain more pairs; dynamic scheduling limits load imbalance.
    #pragma omp parallel for schedule(dynamic, 8) reduction(cp_min:best)
    for (std::ptrdiff_t i = 0; i < rows; ++i)
        best = better(best, scan_row(problem, static_cast<std::size_t>(i)));
    return best;
}
} // namespace cp
