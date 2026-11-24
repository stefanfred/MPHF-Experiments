#pragma once

#include <ThresholdBasedBumpingConsensus.hpp>
#include "Contender.h"

template <uint64_t k>
class ThresholdBasedBumpingConsensusContender : public Contender {
    public:
        using kphf_t = kphf::ThresholdBasedBumpingConsensus::ThresholdBasedBumpingConsensus<k>;
        using RepairMode = kphf::ThresholdBasedBumpingConsensus::RepairMode;
        kphf_t kphf;
        double overload;
        double thresholdBits;
        RepairMode repairMode;

        /**
         * @param overload gamma, every layer has (number of keys of the layer)/(k*gamma) bins
         * @param thresholdBits average number of threshold bits per bin, can be fractional
         * @param loadFactor alpha = N/(k*M). For alpha < 1, keys are only repaired into empty slots
         *        if the layers did not manage to place all keys within the M bins.
         * @param repairMode how to place keys that are left over (auto: hash-and-displace into free slots if there
         *        are enough of them, MPHF + Elias-Fano otherwise)
         */
        ThresholdBasedBumpingConsensusContender(size_t N, double overload, double thresholdBits,
                                                double loadFactor = 1.0, RepairMode repairMode = RepairMode::Auto)
                : Contender(N, k, loadFactor), overload(overload), thresholdBits(thresholdBits),
                  repairMode(repairMode) {
        }

        std::string name() override {
            return std::string("ThresholdBasedBumpingConsensus")
                    + " overload=" + std::to_string(overload)
                    + " thresholdBits=" + std::to_string(thresholdBits)
                    + " repair=" + kphf::ThresholdBasedBumpingConsensus::repairModeName(repairMode);
        }

        void construct(const std::vector<std::string> &keys) override {
            kphf = kphf_t(keys, overload, thresholdBits, loadFactor, repairMode);
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

void thresholdBasedBumpingConsensusContenderRunner(size_t N, size_t k, double loadFactor = 1.0);
