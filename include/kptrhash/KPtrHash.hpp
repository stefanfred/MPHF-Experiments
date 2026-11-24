#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <bytehamster/util/Function.h>
#include <hash128.hpp>
#include <CapacityRepair.hpp>
#include <TruncatedPoisson.hpp>

/**
 * C++ port of k-PtrHash (Ragnar Groot Koerkamp, https://github.com/RagnarGrootKoerkamp/static-hash-sets, crate kphf):
 * a non-minimal k-perfect hash function based on PtrHash. Keys are assigned to buckets with the bucket function
 * x^4, buckets are placed from large to small, and each bucket stores an 8-bit seed: the top bit selects one of two
 * hash functions to a bin and the low 7 bits shift the result by 0..127 bins (so a 128-bit window of a "bin is not
 * full" bit vector checks all shifts at once). Among all seeds that fit, the one with the smallest score is taken.
 *
 * The score of a seed only depends on the loads of the bins that its keys hit: if c keys of the bucket hit a bin
 * with load l, the seed pays cumulative[l + c] - cumulative[l]. Two choices are implemented:
 *  - AdHoc: the weights of the original implementation, the t-th slot of a bin costs 2^t, i.e. cumulative[l] = 2^l - 1.
 *  - Natural: the log of the number of ways in which the remaining keys can still be placed. In the truncated Poisson
 *    model of the lower bound, that number is (up to factors that are the same for all seeds) prod_j S_{k - l_j}(lambda)
 *    with S_e(lambda) = sum_{i <= e} lambda^i / i!, where lambda solves sum_j E[Pois(lambda) | <= k - l_j] = (number of
 *    keys that still have to be placed). So a seed should minimize the loss of log-count,
 *    sum_j [ln S_{k - l_j}(lambda) - ln S_{k - l_j - c_j}(lambda)], i.e. cumulative[l] = -ln S_{k - l}(lambda).
 *    lambda is updated during construction from the histogram of free capacities (an O(k) bisection every n/512
 *    placed keys). The marginal cost of the t-th slot is ln(S_{k-t} / S_{k-t-1}), which grows with t like 2^t does,
 *    but its steepness depends on how full the table is and how many keys are left.
 *  - NaturalStatic: like Natural, but lambda is fixed to its initial value (the lambda* of the lower bound for alpha).
 *
 * Differences to the Rust implementation: the output range is exactly ceil(n/k)/alpha bins (the 128 bins of padding
 * are inside the range), and bumped keys are not placed in a second k-PtrHash with load factor 0.5 behind the range,
 * but into the free slots of the range with CapacityRepair (so that the load factor is the requested one).
 */
namespace kphf::KPtrHash {

enum class Score { AdHoc, Natural, NaturalStatic };

inline std::string scoreName(Score s) {
    switch (s) {
        case Score::AdHoc: return "adhoc";
        case Score::Natural: return "natural";
        case Score::NaturalStatic: return "naturalStatic";
    }
    return "?";
}

/** Space lower bound in bits per key for a k-perfect hash function with load factor alpha. */
inline double spaceLowerBound(uint64_t k, double alpha) {
    const uint64_t n = 1000000;
    std::vector<uint64_t> hist(k + 1, 0);
    hist[k] = uint64_t(double(n) / alpha / double(k));
    return tpois::fillLowerBoundBits(hist, n) / double(n);
}

template <size_t K>
class KPtrHash {
        static_assert(K >= 1 && K <= 255);
        static constexpr uint64_t C = 0x517cc1b727220a95ul;

        uint64_t n = 0;
        uint64_t numBins = 0;
        uint64_t numBuckets = 0;
        uint64_t salt = 0;
        uint64_t padding = 128; // power of two <= 128
        std::vector<uint8_t> seeds;
        bool hasRepair = false;
        size_t numBumped = 0;
        CapacityRepair<> repair;

        static uint64_t mul(uint64_t a, uint64_t b) {
            return uint64_t((__uint128_t(a) * b) >> 64);
        }

        uint64_t hashKey(const Hash128 &h) const {
            return bytehamster::util::remix(h.hi ^ salt);
        }

        uint64_t toBucket(uint64_t x) const {
            uint64_t sq = mul(x, x);
            uint64_t four = mul(sq, sq);
            return mul(four, numBuckets);
        }

        uint64_t toBin(uint64_t x, uint64_t seed) const {
            uint64_t y = (x ^ (seed >> 7)) * C;
            return mul(y, numBins - padding) + (seed & (padding - 1));
        }

        /** Bits [pos, pos + 128) of the bit vector, bit i of the result is bit pos + i */
        static unsigned __int128 window(const std::vector<uint64_t> &bits, uint64_t pos) {
            uint64_t w = pos / 64, o = pos % 64;
            uint64_t a = bits[w], b = bits[w + 1], c = bits[w + 2];
            uint64_t lo = o ? (a >> o) | (b << (64 - o)) : a;
            uint64_t hi = o ? (b >> o) | (c << (64 - o)) : b;
            return (static_cast<unsigned __int128>(hi) << 64) | lo;
        }

    public:
        KPtrHash() = default;

        /**
         * @param alpha load factor, the range is ceil(n/K)/alpha bins
         * @param bitsPerKey target space of the seeds (8 bits per bucket), determines the number of buckets
         * @param score score function to choose among the seeds that fit
         * @param greedy take the first seed that fits instead of the one with the best score
         */
        KPtrHash(const std::vector<std::string> &keys, double alpha, double bitsPerKey, Score score,
                 bool greedy = false) : n(keys.size()) {
            uint64_t minimalBins = (n + K - 1) / K;
            numBins = std::max<uint64_t>(minimalBins, uint64_t(double(minimalBins) / alpha));
            numBins = std::max<uint64_t>(numBins, 2);
            while (padding > 1 && 2 * padding > numBins) padding /= 2;
            numBuckets = std::max<uint64_t>(1, uint64_t(std::ceil(double(n) * bitsPerKey / 8.0)));

            std::vector<Hash128> hashes;
            hashes.reserve(n);
            for (const std::string &key : keys) hashes.emplace_back(key);

            std::mt19937_64 rng(0x1234567);
            for (int attempt = 0; attempt < 10; attempt++) {
                salt = rng();
                if (build(hashes, score, greedy)) return;
            }
            throw std::runtime_error("KPtrHash: construction failed");
        }

    private:
        bool build(const std::vector<Hash128> &hashes, Score score, bool greedy) {
            // 1. Sort keys by bucket (counting sort)
            std::vector<uint64_t> bucketStarts(numBuckets + 1, 0);
            for (const Hash128 &h : hashes) bucketStarts[toBucket(hashKey(h)) + 1]++;
            for (uint64_t i = 0; i < numBuckets; i++) bucketStarts[i + 1] += bucketStarts[i];
            std::vector<std::pair<uint64_t, uint32_t>> bucketed(n); // (x, index of key)
            {
                std::vector<uint64_t> pos(bucketStarts.begin(), bucketStarts.end() - 1);
                for (uint32_t i = 0; i < n; i++) {
                    uint64_t x = hashKey(hashes[i]);
                    bucketed[pos[toBucket(x)]++] = {x, i};
                }
            }
            // 2. Buckets by decreasing size
            std::vector<uint32_t> perm(numBuckets);
            for (uint32_t i = 0; i < numBuckets; i++) perm[i] = i;
            std::stable_sort(perm.begin(), perm.end(), [&](uint32_t a, uint32_t b) {
                return bucketStarts[a + 1] - bucketStarts[a] > bucketStarts[b + 1] - bucketStarts[b];
            });

            std::vector<uint8_t> binSizes(numBins, 0);
            std::vector<uint64_t> nonFull((numBins + 63) / 64 + 3, 0);
            for (uint64_t b = 0; b < numBins; b++) nonFull[b / 64] |= 1ul << (b % 64);
            seeds.assign(numBuckets, 255); // 0 means bumped

            // Score tables, cumulative[l] for l = 0..K
            std::array<double, K + 1> cumulative{};
            std::vector<uint64_t> hist(K + 1, 0); // hist[e] = #bins with e free slots
            hist[K] = numBins;
            uint64_t remaining = n;
            uint64_t lastUpdate = ~0ul;
            uint64_t updateInterval = std::max<uint64_t>(K, n / 512);
            std::vector<double> lnS;
            auto updateNatural = [&]() {
                double lambda = tpois::solveLambda(hist, double(std::max<uint64_t>(remaining, 1)));
                tpois::logPartialSums(lambda, K, lnS);
                for (size_t l = 0; l <= K; l++) cumulative[l] = -lnS[K - l];
                lastUpdate = remaining;
            };
            if (score == Score::AdHoc) {
                for (size_t l = 0; l <= K; l++) cumulative[l] = std::ldexp(1.0, int(l)) - 1.0;
            } else {
                updateNatural();
            }

            std::vector<uint64_t> bins;
            std::vector<std::pair<uint64_t, uint32_t>> binCounts;
            std::vector<uint64_t> bumped;
            numBumped = 0;

            for (uint64_t i = 0; i < numBuckets; i++) {
                uint32_t idx = perm[i];
                uint64_t start = bucketStarts[idx], end = bucketStarts[idx + 1];
                if (start == end) continue;
                if (score == Score::Natural && lastUpdate - remaining >= updateInterval) {
                    updateNatural();
                }

                double bestScore = std::numeric_limits<double>::infinity();
                int bestSeed = -1;
                unsigned __int128 mask = 0;
                bool impossible = false;
                for (int seed = 0; seed < 256; seed++) {
                    if (seed % 128 == 0) {
                        bins.clear();
                        binCounts.clear();
                        for (uint64_t j = start; j < end; j++) bins.push_back(toBin(bucketed[j].first, seed));
                        std::sort(bins.begin(), bins.end());
                        uint32_t maxCount = 0;
                        for (size_t j = 0; j < bins.size();) {
                            size_t j2 = j;
                            while (j2 < bins.size() && bins[j2] == bins[j]) j2++;
                            binCounts.emplace_back(bins[j], uint32_t(j2 - j));
                            maxCount = std::max<uint32_t>(maxCount, j2 - j);
                            j = j2;
                        }
                        impossible = maxCount > K;
                        mask = ~static_cast<unsigned __int128>(0);
                        for (auto &[bi, count] : binCounts) mask &= window(nonFull, bi);
                    }
                    uint64_t shift = seed % 128;
                    if (impossible || shift >= padding) continue;
                    if (seed == 0) continue; // reserved for bumping
                    if (((mask >> shift) & 1) == 0) continue;
                    if (i == 0 && seed != 1 && shift != 0) continue; // all shifts are equivalent for the first bucket

                    double s = 0;
                    bool ok = true;
                    for (auto &[bi, count] : binCounts) {
                        uint64_t load = binSizes[bi + shift];
                        if (load + count > K) { ok = false; break; }
                        s += cumulative[load + count] - cumulative[load];
                    }
                    if (!ok) continue;
                    if (s < bestScore) {
                        bestScore = s;
                        bestSeed = seed;
                    }
                    if (greedy) break;
                }

                remaining -= end - start;
                if (bestSeed < 0) {
                    if (i == 0 && n >= 1000000) return false; // Never bump the largest bucket
                    seeds[idx] = 0;
                    for (uint64_t j = start; j < end; j++) {
                        const Hash128 &h = hashes[bucketed[j].second];
                        bumped.push_back(h.hi ^ h.lo);
                    }
                    numBumped += end - start;
                    continue;
                }
                seeds[idx] = uint8_t(bestSeed);
                for (uint64_t j = start; j < end; j++) {
                    uint64_t bi = toBin(bucketed[j].first, bestSeed);
                    uint8_t &load = binSizes[bi];
                    hist[K - load]--;
                    load++;
                    hist[K - load]++;
                    if (load == K) nonFull[bi / 64] &= ~(1ul << (bi % 64));
                }
            }

            hasRepair = !bumped.empty();
            if (hasRepair) {
                std::vector<uint32_t> loads(binSizes.begin(), binSizes.end());
                repair = CapacityRepair<>(bumped, std::move(loads), K);
            }
            return true;
        }

    public:
        uint64_t operator()(const std::string &key) const {
            return operator()(Hash128(key));
        }

        uint64_t operator()(const Hash128 &h) const {
            uint64_t x = hashKey(h);
            uint64_t seed = seeds[toBucket(x)];
            if (seed == 0) [[unlikely]] {
                return repair(h.hi ^ h.lo);
            }
            return toBin(x, seed);
        }

        [[nodiscard]] size_t count_bits() const {
            return 8 * sizeof(*this) + 8 * seeds.capacity()
                   + (hasRepair ? repair.count_bits() - 8 * sizeof(repair) : 0);
        }

        [[nodiscard]] size_t bumpedKeys() const { return numBumped; }
        [[nodiscard]] size_t repairBits() const { return hasRepair ? repair.count_bits() : 0; }
};

}
