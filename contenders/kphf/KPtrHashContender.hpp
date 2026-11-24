#pragma once

#include <KPtrHash.hpp>
#include "Contender.h"

template <size_t k>
class KPtrHashContender : public Contender {
    public:
        using Score = kphf::KPtrHash::Score;
        kphf::KPtrHash::KPtrHash<k> kphf;
        double bitsPerKey;
        Score score;
        bool greedy;

        /**
         * @param bitsPerKey target bits per key of the 8-bit seeds (determines the number of buckets)
         * @param score adhoc (2^load slot weights of the original) or natural (log-count of the remaining placements)
         * @param greedy take the first seed that fits instead of the best scoring one
         */
        KPtrHashContender(size_t N, double loadFactor, double bitsPerKey, Score score, bool greedy = false)
                : Contender(N, k, loadFactor), bitsPerKey(bitsPerKey), score(score), greedy(greedy) {
        }

        std::string name() override {
            return std::string("KPtrHash")
                    + " bitsPerKey=" + std::to_string(bitsPerKey)
                    + " score=" + (greedy ? std::string("greedy") : kphf::KPtrHash::scoreName(score));
        }

        void construct(const std::vector<std::string> &keys) override {
            kphf = kphf::KPtrHash::KPtrHash<k>(keys, loadFactor, bitsPerKey, score, greedy);
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

void kPtrHashContenderRunner(size_t N, size_t k, double loadFactor);
