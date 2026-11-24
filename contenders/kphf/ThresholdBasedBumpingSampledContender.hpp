#pragma once

#include <ThresholdBasedBumpingSampled.hpp>
#include "Contender.h"

/** FixedThreshold: one threshold per layer, the bin's bits only change the fingerprints */
template <uint64_t k, bool FixedThreshold = false>
class ThresholdBasedBumpingSampledContender : public Contender {
    public:
        using kphf_t = kphf::ThresholdBasedBumpingSampled::ThresholdBasedBumpingSampled<k, FixedThreshold>;
        kphf_t kphf;
        double overload;
        double slack;

        /**
         * @param overload gamma, every layer has (number of keys of the layer)/(k*gamma) bins
         * @param slack threshold bits per bin = slack * model cost (+0.02), raised automatically if the consensus
         *        search does not finish. Larger is faster to construct.
         */
        ThresholdBasedBumpingSampledContender(size_t N, double overload, double loadFactor, double slack = 1.3)
                : Contender(N, k, loadFactor), overload(overload), slack(slack) {
        }

        std::string name() override {
            return std::string(FixedThreshold ? "ThresholdBasedBumpingFixed" : "ThresholdBasedBumpingSampled")
                    + " overload=" + std::to_string(overload)
                    + " slack=" + std::to_string(slack)
                    + " thresholdBitsUsed=" + std::to_string(kphf.thresholdBitsUsed())
                    + " modelCost=" + std::to_string(kphf.modelCost());
        }

        void construct(const std::vector<std::string> &keys) override {
            kphf = kphf_t(keys, overload, 0.0, loadFactor, kphf::ThresholdBasedBumpingSampled::RepairMode::Auto, slack);
        }

        size_t sizeBits() override {
            return kphf.count_bits();
        }

        void performQueries(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return kphf(key);
            };
            doPerformQueries(keys, x);
        }

        void performTest(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return kphf(key);
            };
            doPerformTest(keys, x);
        }

        size_t lookup(std::string &key) override {
            return kphf(key);
        }

        bool hasLookup() const override { return true; }
};

void thresholdBasedBumpingSampledContenderRunner(size_t N, size_t k, double loadFactor, bool fixedThreshold = false);
