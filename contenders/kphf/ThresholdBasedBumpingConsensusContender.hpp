#pragma once

#include <ThresholdBasedBumpingConsensus.hpp>
#include "Contender.h"

template <uint64_t k, int threshold_size>
class ThresholdBasedBumpingConsensusContender : public Contender {
    public:
        using kphf_t = kphf::ThresholdBasedBumpingConsensus::ThresholdBasedBumpingConsensus<k, threshold_size>;
        kphf_t kphf;
        double overload;

        /**
         * @param overload gamma, every layer has (number of keys of the layer)/(k*gamma) bins
         * @param loadFactor alpha = N/(k*M). For alpha < 1, keys are only repaired into empty slots
         *        if the layers did not manage to place all keys within the M bins.
         */
        explicit ThresholdBasedBumpingConsensusContender(size_t N, double overload, double loadFactor = 1.0)
                : Contender(N, k, loadFactor), overload(overload) {
        }

        std::string name() override {
            return std::string("ThresholdBasedBumpingConsensus")
                    + " overload=" + std::to_string(overload)
                    + " thresholdSize=" + std::to_string(threshold_size);
        }

        void construct(const std::vector<std::string> &keys) override {
            kphf = kphf_t(keys, overload, loadFactor);
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
            auto x = [&] (std::string &key) {
                return kphf(key);
            };
            return x(key);
        }

        bool hasLookup() const override { return true; }
};

void thresholdBasedBumpingConsensusContenderRunner(size_t N, size_t k, double loadFactor = 1.0);
