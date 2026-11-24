#include "ThresholdBasedBumpingConsensusContender.hpp"

#include <tlx/math/integer_log2.hpp>
#include <DispatchK.h>

/**
 * Overload factors gamma to try. For the minimal case (alpha=1), every layer has to overload its bins (gamma > 1).
 * For alpha < 1, the layers can place all keys without repair as soon as E[min(Pois(gamma*k), k)] >= alpha*k,
 * which can already happen for gamma < 1 if k is large. Larger gamma saves space (until it saturates) but makes
 * queries visit more layers (about gamma/alpha in expectation).
 */
inline std::vector<double> consensusOverloads(double loadFactor) {
    if (loadFactor > 1.0 - 1e-9) {
        return {1.05, 1.10, 1.20, 1.30, 1.40, 1.50, 1.60, 1.80, 2.00, 2.20};
    }
    std::vector<double> result;
    for (double gamma : {0.80, 0.90, 1.00, 1.05, 1.10, 1.20, 1.30, 1.40, 1.50, 1.60, 1.80, 2.00, 2.20, 2.50, 3.00}) {
        if (gamma >= loadFactor - 1e-9) {
            result.push_back(gamma);
        }
    }
    return result;
}

template <size_t k, size_t threshold_size>
void dispatchOverloadsConsensus(size_t N, double loadFactor) {
    for (double overload : consensusOverloads(loadFactor)) {
        ThresholdBasedBumpingConsensusContender<k, threshold_size>(N, overload, loadFactor).run();
    }
}

template <size_t k>
struct ThresholdBasedBumpingConsensusContenderRunner {
    void operator() (size_t N, double loadFactor) const {
        constexpr size_t firstSize = tlx::integer_log2_ceil(k) >= 6 ? tlx::integer_log2_ceil(k) - 4 : 2;
        dispatchOverloadsConsensus<k, firstSize>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 1>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 2>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 3>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 4>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 5>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 6>(N, loadFactor);
        dispatchOverloadsConsensus<k, firstSize + 7>(N, loadFactor);
    }
};

void thresholdBasedBumpingConsensusContenderRunner(size_t N, size_t k, double loadFactor) {
    dispatchK<ThresholdBasedBumpingConsensusContenderRunner>(k, N, loadFactor);
}
