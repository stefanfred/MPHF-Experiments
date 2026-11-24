#include "ThresholdBasedBumpingSampledContender.hpp"

#include "DispatchK.h"

template <size_t k>
struct ThresholdBasedBumpingSampledContenderRunner {
    void operator() (size_t N, double loadFactor) const {
        if constexpr (k <= 128) {
            std::vector<double> overloads = loadFactor > 1.0 - 1e-9
                    ? std::vector<double>{1.1, 1.3, 1.5, 2.0, 2.5}
                    : std::vector<double>{1.0, 1.3, 1.5, 2.0, 2.5};
            for (double overload : overloads) {
                for (double slack : {1.3, 2.0}) {
                    ThresholdBasedBumpingSampledContender<k>(N, overload, loadFactor, slack).run();
                }
            }
        } else {
            (void) N; (void) loadFactor;
        }
    }
};

void thresholdBasedBumpingSampledContenderRunner(size_t N, size_t k, double loadFactor) {
    dispatchK<ThresholdBasedBumpingSampledContenderRunner>(k, N, loadFactor);
}
