#include "ThresholdBasedBumpingSampledContender.hpp"

#include "DispatchK.h"

template <size_t k>
struct ThresholdBasedBumpingSampledContenderRunner {
    void operator() (size_t N, double loadFactor, bool fixedThreshold) const {
        if constexpr (k <= 128) {
            std::vector<double> overloads = loadFactor > 1.0 - 1e-9
                    ? std::vector<double>{1.1, 1.3, 1.5, 2.0, 2.5}
                    : std::vector<double>{1.0, 1.3, 1.5, 2.0, 2.5};
            for (double overload : overloads) {
                for (double slack : {1.3, 2.0}) {
                    if (fixedThreshold) {
                        ThresholdBasedBumpingSampledContender<k, true>(N, overload, loadFactor, slack).run();
                    } else {
                        ThresholdBasedBumpingSampledContender<k, false>(N, overload, loadFactor, slack).run();
                    }
                }
            }
        } else {
            (void) N; (void) loadFactor; (void) fixedThreshold;
        }
    }
};

void thresholdBasedBumpingSampledContenderRunner(size_t N, size_t k, double loadFactor, bool fixedThreshold) {
    dispatchK<ThresholdBasedBumpingSampledContenderRunner>(k, N, loadFactor, fixedThreshold);
}
