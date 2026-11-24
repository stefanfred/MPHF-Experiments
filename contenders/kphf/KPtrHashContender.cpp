#include "KPtrHashContender.hpp"

#include "DispatchK.h"

template <size_t k>
struct KPtrHashContenderRunner {
    void operator() (size_t N, double loadFactor) const {
        if constexpr (k <= 255) {
            using Score = kphf::KPtrHash::Score;
            // Like the Rust benchmark: a target space relative to the lower bound, plus some absolute values for
            // large k where the lower bound is tiny (8 bits per bucket need buckets of at most a few hundred keys).
            double lb = kphf::KPtrHash::spaceLowerBound(k, loadFactor);
            std::vector<double> targets;
            for (double f : {1.2, 1.35, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0}) targets.push_back(f * lb);
            for (double t : {0.05, 0.1, 0.2}) targets.push_back(t);
            for (double bitsPerKey : targets) {
                if (bitsPerKey < 8.0 / 200) continue; // at most ~200 keys per bucket
                for (Score score : {Score::AdHoc, Score::Natural, Score::NaturalStatic}) {
                    KPtrHashContender<k>(N, loadFactor, bitsPerKey, score).run();
                }
            }
        } else {
            (void) N; (void) loadFactor;
        }
    }
};

void kPtrHashContenderRunner(size_t N, size_t k, double loadFactor) {
    dispatchK<KPtrHashContenderRunner>(k, N, loadFactor);
}
