#pragma once

#include <HashDisplace.hpp>
#include "Contender.h"

template<size_t k, typename BucketFunction, typename Encoding>
class HashDisplaceContender : public Contender {
    public:
        kphf::HashDisplace::HashDisplace<k, BucketFunction, Encoding> kphf;
        size_t bucketSize;

        /**
         * @param loadFactor alpha = N/(k*M). For alpha < 1, the optimal bucket function only uses its [0, alpha]
         *        prefix (keys are hashed to [0, alpha) instead of [0, 1)), see OptimalBucketFunction.
         */
        HashDisplaceContender(const size_t N, const size_t bucketSize, const double loadFactor = 1.0)
                : Contender(N, k, loadFactor), bucketSize(bucketSize) {
        }

        std::string name() override {
            return std::string("HashDisplace")
                    + " bucketSize=" + std::to_string(bucketSize)
                    + " bucketFunction=" + BucketFunction::name()
                    + " encoding=" + Encoding::name();
        }

        void construct(const std::vector<std::string> &keys) override {
            kphf = kphf::HashDisplace::HashDisplace<k, BucketFunction, Encoding>(keys, bucketSize, loadFactor);
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

void hashDisplaceContenderRunner(size_t N, size_t k, double loadFactor = 1.0);
