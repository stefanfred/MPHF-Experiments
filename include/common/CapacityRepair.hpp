#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

#include <bytehamster/util/Function.h>
#include <RiceEncoding.hpp>
#include "TruncatedPoisson.hpp"

namespace kphf {

/**
 * Generalized static function into free slots: maps a set of keys to bins such that bin j receives at most
 * (k - loads[j]) keys. Which key goes to which free slot is free to choose, and the query does not know the loads.
 *
 * Lower bound (counting, i.e. brute force over the seeds of a random function is optimal):
 * -log2 P[a random function respects all capacities], see tpois::fillLowerBoundBits.
 * Placing the keys one after the other with an ideally coded seed each already matches it (chain rule:
 * P = prod_i P[key i hits a bin with room | keys before placed]).
 *
 * Algorithm: hash-and-displace with free capacities. Keys are grouped into buckets of expected size bucketSize,
 * buckets are placed from large to small, each with the first seed that maps all of its keys to bins with free
 * slots, and the seeds are Rice-coded. A bucket of b keys succeeds with probability about rho^b (rho = fraction of
 * bins with free slots), so its seed costs about b log2(1/rho) + O(1) bits, which is close to the lower bound as
 * long as rho does not get small, i.e. as long as a constant fraction of the free slots stays free.
 * (An adaptive bucket size following rho was tried, but processing buckets large to small matters more.)
 *
 * This replaces the MPHF + Elias-Fano repair, which needs about log2 e + 2 + log2(B/n) bits per key even if most
 * bins have free slots. If (almost) every free slot has to be used, the last keys need very specific bins and the
 * search gets slow, so use the old method in that case (see isSuitable).
 */
template <typename Encoding = HashDisplace::RiceEncoding>
class CapacityRepair {
    private:
        uint64_t nbins = 0;
        uint64_t nbuckets = 0;
        Encoding seeds;

        static uint64_t bucketOf(uint64_t key, uint64_t nb) {
            return bytehamster::util::fastrange64(bytehamster::util::remix(key ^ 0x2545F4914F6CDD1Dul), nb);
        }

        static uint64_t binOf(uint64_t key, uint64_t seed, uint64_t nb) {
            return bytehamster::util::fastrange64(
                    bytehamster::util::remix(key + (seed + 1) * 0x9E3779B97F4A7C15ul), nb);
        }

    public:
        CapacityRepair() = default;

        /** The method is efficient if a constant fraction of the free slots stays free. */
        static bool isSuitable(uint64_t numKeys, uint64_t freeSlots) {
            return double(freeSlots) >= 1.25 * double(numKeys);
        }

        /**
         * @param keys 64-bit hashes of the keys to place
         * @param loads current number of keys in each bin (a copy is updated internally)
         * @param k capacity of every bin
         * @param bucketSize expected number of keys per seed. Larger is closer to the lower bound (the overhead
         *        is about 1.5/bucketSize bits per key) but the search gets exponentially slower.
         *        0 chooses it automatically, see autoBucketSize.
         */
        CapacityRepair(const std::vector<uint64_t> &keys, std::vector<uint32_t> loads, uint64_t k,
                       double bucketSize = 0)
                : nbins(loads.size()) {
            if (bucketSize <= 0) {
                bucketSize = autoBucketSize(loads, k, keys.size());
            }
            nbuckets = std::max<uint64_t>(1, uint64_t(std::ceil(double(keys.size()) / bucketSize)));
            std::vector<std::pair<uint64_t, uint64_t>> sorted; // (bucket, key)
            sorted.reserve(keys.size());
            for (uint64_t key : keys) sorted.emplace_back(bucketOf(key, nbuckets), key);
            std::sort(sorted.begin(), sorted.end());
            struct Bucket { uint64_t idx, begin, end; };
            std::vector<Bucket> buckets;
            for (size_t i = 0; i < sorted.size();) {
                size_t j = i;
                while (j < sorted.size() && sorted[j].first == sorted[i].first) j++;
                buckets.push_back({sorted[i].first, i, j});
                i = j;
            }
            std::stable_sort(buckets.begin(), buckets.end(), [](const Bucket &a, const Bucket &b) {
                return a.end - a.begin > b.end - b.begin;
            });
            std::vector<uint64_t> seedsVec(nbuckets, 0);
            for (const Bucket &b : buckets) {
                for (uint64_t seed = 0;; seed++) {
                    if (seed > (1ul << 40)) {
                        throw std::runtime_error("CapacityRepair: no seed found");
                    }
                    uint64_t j = b.begin;
                    for (; j < b.end; j++) {
                        uint64_t bin = binOf(sorted[j].second, seed, nbins);
                        if (loads[bin] >= k) break;
                        loads[bin]++;
                    }
                    if (j == b.end) {
                        seedsVec[b.idx] = seed;
                        break;
                    }
                    while (j > b.begin) {
                        --j;
                        loads[binOf(sorted[j].second, seed, nbins)]--;
                    }
                }
            }
            seeds = Encoding(seedsVec);
        }

        uint64_t operator()(uint64_t key) const {
            return binOf(key, seeds[bucketOf(key, nbuckets)], nbins);
        }

        [[nodiscard]] size_t count_bits() const {
            return 8 * sizeof(*this) + seeds.count_bits() - 8 * sizeof(seeds);
        }

        [[nodiscard]] uint64_t numBuckets() const { return nbuckets; }

        /**
         * Bucket size such that a bucket needs about 2^targetBits seeds when the fraction `point` of the keys is
         * placed: b = targetBits / log2(1/rho), where rho is the predicted fraction of bins with free slots at that
         * point. The prediction assumes that every bin receives Pois(tau) placement attempts that succeed while it
         * has room (keys are spread over bins with room uniformly, not proportional to their free slots), with tau
         * such that sum_j E[min(Pois(tau), e_j)] = point * n.
         */
        static double autoBucketSize(const std::vector<uint32_t> &loads, uint64_t k, uint64_t nKeys,
                                     double targetBits = 6.0, double point = 0.75) {
            std::vector<double> hist(k + 1, 0);
            for (uint32_t l : loads) hist[k - std::min<uint64_t>(l, k)]++;
            if (loads.empty()) return 1;
            auto stats = [&](double tau, double &placed, double &room) {
                placed = 0; room = 0;
                for (size_t e = 1; e <= k; e++) {
                    if (hist[e] == 0) continue;
                    // P[Pois(tau) = i] for i < e, E[min(X, e)] = sum_{i<e} i p_i + e P[X >= e]
                    double p = std::exp(-tau), cdf = 0, partial = 0;
                    for (size_t i = 0; i < e; i++) {
                        cdf += p;
                        partial += double(i) * p;
                        p *= tau / double(i + 1);
                    }
                    placed += hist[e] * (partial + double(e) * (1.0 - cdf));
                    room += hist[e] * cdf;
                }
            };
            double lo = 0, hi = 1;
            double placed, room;
            for (stats(hi, placed, room); placed < point * double(nKeys) && hi < 1e6; stats(hi, placed, room)) hi *= 2;
            for (int it = 0; it < 60; it++) {
                double mid = 0.5 * (lo + hi);
                stats(mid, placed, room);
                if (placed < point * double(nKeys)) lo = mid; else hi = mid;
            }
            stats(hi, placed, room);
            double rho = room / double(loads.size());
            if (rho >= 1.0) return 32;
            return std::clamp(targetBits / -std::log2(rho), 2.0, 32.0);
        }

        /** Lower bound in bits for placing nKeys keys into bins with the given loads. */
        static double lowerBoundBits(const std::vector<uint32_t> &loads, uint64_t k, uint64_t nKeys) {
            std::vector<uint64_t> hist(k + 1, 0);
            for (uint32_t l : loads) hist[k - std::min<uint64_t>(l, k)]++;
            return tpois::fillLowerBoundBits(hist, nKeys);
        }
};

}
