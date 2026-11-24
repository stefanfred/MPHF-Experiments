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
    for (double gamma : {0.80, 0.90, 1.00, 1.10, 1.20, 1.40, 1.60, 2.00, 2.50}) {
        if (gamma >= loadFactor - 1e-9) {
            result.push_back(gamma);
        }
    }
    return result;
}

/**
 * Average threshold bits per bin (fractional, as in the consensus paper). For alpha < 1, a bin needs fewer bits
 * because more empty slots are allowed, so we also go below 2 bits (bins with 0 or 1 bit use small codebooks).
 */
inline std::vector<double> consensusThresholdBits(size_t k, double loadFactor) {
    size_t firstSize = tlx::integer_log2_ceil(k) >= 6 ? tlx::integer_log2_ceil(k) - 4 : 2;
    std::vector<double> result;
    if (loadFactor < 1.0 - 1e-9) {
        for (double b : {0.25, 0.5, 1.0, 1.5}) {
            if (b < double(firstSize)) result.push_back(b);
        }
    }
    for (double b = firstSize; b <= firstSize + 7 + 1e-9; b += 0.5) {
        result.push_back(b);
    }
    return result;
}

template <size_t k>
struct ThresholdBasedBumpingConsensusContenderRunner {
    void operator() (size_t N, double loadFactor) const {
        for (double bits : consensusThresholdBits(k, loadFactor)) {
            for (double overload : consensusOverloads(loadFactor)) {
                ThresholdBasedBumpingConsensusContender<k>(N, overload, bits, loadFactor).run();
            }
        }
    }
};

void thresholdBasedBumpingConsensusContenderRunner(size_t N, size_t k, double loadFactor) {
    dispatchK<ThresholdBasedBumpingConsensusContenderRunner>(k, N, loadFactor);
}
