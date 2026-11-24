#pragma once

#include <vector>
#include <limits>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <ranges>

#include <sux/bits/EliasFano.hpp>
#include <ips2ra.hpp>
#include <bytehamster/util/Function.h>
#include <Fips.h>

#include <hash128.hpp>
#include <CapacityRepair.hpp>
#include "consensus.hpp"
#include "optimalThresholds.hpp"

namespace kphf::ThresholdBasedBumpingConsensus {

struct ThresholdInfo {
    double threshold;
    double l1, l2;

    ThresholdInfo(double t): threshold(t), l1(log(t)), l2(log(1-t)) {}
};

inline std::vector<ThresholdInfo> compute_threshold_info(const std::vector<double> &thresholds) {
    std::vector<ThresholdInfo> threshold_info;
    for (double t: thresholds) {
        threshold_info.emplace_back(t);
    }
    return threshold_info;
}

inline std::vector<double> compute_lgamma_cache(uint64_t limit) {
    std::vector<double> r(limit + 2);
    for (size_t i = 0; i < r.size(); i++) {
        r[i] = lgamma(i);
    }
    return r;
}

inline double success_exp(uint64_t n, uint64_t k, const std::vector<ThresholdInfo> &thresholds,
        const std::vector<double> &lgamma_cache) {
    if (k > n) return 0;
    double lbinom = lgamma_cache[n + 1] - lgamma_cache[k + 1] - lgamma_cache[n - k + 1];
    double e = 0;
    for (const auto &[threshold, l1, l2] : thresholds) {
        if (std::isinf(l1)) {
            e += (k == 0);
        } else if (std::isinf(l2)) {
            e += (k == n);
        } else {
            e += exp(lbinom + k * l1 + (n - k) * l2);
        }
    }
    return e;
}

inline double best_error(uint64_t n, size_t k, const std::vector<ThresholdInfo> &thresholds, const std::vector<double> &lgamma_cache) {
    uint64_t goal = std::min(n, k);
    constexpr double TUNE = 1.05;
    double need = TUNE;
    need -= success_exp(n, goal, thresholds, lgamma_cache);
    if (need <= 0) {
        return 0;
    }
    double avg = 0;
    for (uint64_t error = 1; error <= goal; error++) {
        double s = success_exp(n, goal-error, thresholds, lgamma_cache);
        if (s >= need) {
            return error - 1 + need/s;
        }
        need -= s;
        avg += error * s;
    }
    return goal;
}

struct AllowedImperfectBumpingEntry {
    uint64_t allowedBumped;
    uint64_t probability;
};

/**
 * Allowed number of empty slots per bin size for one threshold codebook (runtime version).
 * Codebooks with at most 2 thresholds (fractional threshold lengths below 2) allow any number of empty slots:
 * the search tries the largest threshold first anyway, and the other one is the alternative for backtracking.
 */
struct AllowedImperfectBumping {
    const uint64_t k;
    const uint64_t max_n;
    const bool anyError;
    std::vector<ThresholdInfo> threshold_info;
    std::vector<double> lgamma_cache;
    std::vector<AllowedImperfectBumpingEntry> errors;

    AllowedImperfectBumping(uint64_t k, double lambda, const std::vector<double> &thresholds)
        : k(k), max_n(static_cast<uint64_t>(2ul * k * lambda) + 2), anyError(thresholds.size() <= 2) {
        threshold_info = compute_threshold_info(thresholds);
        lgamma_cache = compute_lgamma_cache(max_n);
        errors.resize(max_n + 1, {~0ul, 0});
    }

    AllowedImperfectBumpingEntry getAllowed(size_t actualBucketSize) {
        if (anyError) {
            return {k + 1, 0};
        }
        if (errors[actualBucketSize].allowedBumped == ~0ul) {
            double e = best_error(actualBucketSize, k, threshold_info, lgamma_cache);
            uint64_t q = static_cast<uint64_t>(e);
            errors[actualBucketSize] = {q+1, ThresholdBasedBumping::double_to_u64(e-q)};
        }
        return errors[actualBucketSize];
    }

    size_t maxBucketSize() const {
        return max_n;
    }
};

/**
 * Expected consensus cost E_Y[log2(m / E[#valid thresholds | Y])] in bits of a small codebook with m <= 2 entries,
 * for Y ~ Pois(lambda) keys per bin, where a threshold is valid if it keeps at most k keys (small codebooks allow any
 * number of empty slots, see best_error). With threshold T, the number of kept keys is Bin(Y, T).
 */
inline double small_codebook_cost(uint64_t k, double lambda, const std::vector<double> &codebook,
                                  double *tailCost = nullptr) {
    uint64_t maxY = uint64_t(lambda + 12 * std::sqrt(lambda) + 20);
    double cost = 0, pY = std::exp(-lambda), mass = 0, cdf = 0;
    if (tailCost) *tailCost = 0;
    for (uint64_t y = 0; y <= maxY; y++) {
        double valid = 0;
        for (double t : codebook) {
            if (y <= k || t >= 1.0) {
                valid += y <= k ? 1.0 : 0.0;
                continue;
            }
            // P[Bin(y, t) <= k]
            double p = 0;
            for (uint64_t i = 0; i <= k; i++) {
                p += std::exp(std::lgamma(y + 1.0) - std::lgamma(i + 1.0) - std::lgamma(y - i + 1.0)
                              + double(i) * std::log(t) + double(y - i) * std::log1p(-t));
            }
            valid += std::min(1.0, p);
        }
        double c = std::log2(double(codebook.size()) / std::max(valid, 1e-300));
        cost += pY * c;
        mass += pY;
        cdf += pY;
        if (tailCost && cdf <= 0.995) *tailCost = std::max(*tailCost, c);
        pY *= lambda / double(y + 1);
    }
    return cost / mass;
}

/**
 * Normalized threshold codebook with 2^length entries for bins with Pois(lambda) keys.
 * For >= 4 thresholds, this is the empty-slot minimizing codebook from the paper (first threshold 0, last 1).
 * Smaller codebooks (used by fractional threshold lengths below 2 bits) cannot afford the "bump everything"
 * threshold 0 and allow any number of empty slots. 2 thresholds are {T, 1}, a single threshold is {T}. T is the
 * largest value such that the expected consensus cost of the codebook (small_codebook_cost) is at most
 * length - 0.4 bits (0.005 bits for length 0), and at most 3 bits for all but the 0.5% largest bins, so
 * the consensus search has enough slack and no single bin needs a lot of backtracking.
 * A single threshold has to be < 1, because bin sizes are fixed per layer and only fingerprints change with the
 * consensus seed, so a bin with more than k keys could never be fixed otherwise.
 */
inline std::vector<double> threshold_codebook(uint64_t k, double lambda, uint64_t length, double targetMu = 0) {
    using ThresholdBasedBumping::compute_thresholds_normalized;
    if (length >= 2) {
        return compute_thresholds_normalized(k, lambda, 1ul << length);
    }
    auto make = [&](double t) { return length == 1 ? std::vector<double>{t, 1.0} : std::vector<double>{t}; };
    if (targetMu > 0) {
        // Load-factor aware: a single threshold T = mu/lambda thins the Pois(lambda) keys of a bin to about
        // Pois(mu), and accepting every bin with at most k kept keys leaves E[Pois(mu) | <= k] = alpha k keys in
        // it. That is the "upper curve" -log2 P[Pois(mu) <= k] / (alpha k) bits per key, close to the lower bound
        // for small alpha. Capped so that no typical bin needs more than 3 bits of backtracking.
        double tCap = 0, lo = 0, hi = 1;
        for (int it = 0; it < 40; it++) {
            double mid = 0.5 * (lo + hi), tail;
            small_codebook_cost(k, lambda, {mid}, &tail);
            if (tail <= 3.0) lo = mid; else hi = mid;
        }
        tCap = lo;
        double t = std::clamp(targetMu / lambda, 1e-6, tCap);
        return length == 1 ? std::vector<double>{0.6 * t, t} : std::vector<double>{t};
    }
    double budget = double(length) - 0.4;
    double lo = 0.0, hi = 1.0;
    for (int it = 0; it < 40; it++) {
        double mid = 0.5 * (lo + hi);
        double tail;
        double cost = small_codebook_cost(k, lambda, make(mid), &tail);
        if (cost <= std::max(budget, 0.005) && tail <= 3.0) lo = mid;
        else hi = mid;
    }
    return make(std::max(lo, 1e-6));
}

enum class RepairMode {
    /** Capacity-aware hash-and-displace if enough free slots remain, otherwise FiPS + Elias-Fano */
    Auto,
    /** Minimal perfect hash function (FiPS) on the leftover keys + Elias-Fano of the empty slots (from the paper) */
    FipsEliasFano,
    /** Hash-and-displace into the bins with free slots (CapacityRepair) */
    Displace
};

inline std::string repairModeName(RepairMode m) {
    switch (m) {
        case RepairMode::Auto: return "auto";
        case RepairMode::FipsEliasFano: return "fipsEF";
        case RepairMode::Displace: return "displace";
    }
    return "?";
}

#ifdef STATS
uint64_t perfect_thresholds = 0, total_thresholds = 0, extra_bumped = 0;
uint64_t bumped_keys = 0, total_keys = 0;
uint64_t filled_buckets = 0, overfull_buckets = 0;

void reset_stats() {
    perfect_thresholds = total_thresholds = extra_bumped = 0;
    bumped_keys = total_keys = 0;
    filled_buckets = overfull_buckets = 0;
}

void dump_stats() {
    std::cout << "Perfect thresholds: "
        << 100.0 * perfect_thresholds / total_thresholds << "%\n";
    std::cout << "Avg. extra bumped: "
        << (double) extra_bumped / total_thresholds << "\n";
    std::cout << "Bumped keys: " << 100.0 * bumped_keys / total_keys << "%\n";
    std::cout << "Filled buckets: " << 100.0 * filled_buckets / total_thresholds << "%\n";
    std::cout << "Overfull buckets: " << 100.0 * overfull_buckets / total_thresholds << "%\n";
}
#endif

struct Key {
    uint64_t bucket, fingerprint;
    Hash128 hash;
};

inline void sort_buckets(std::vector<Key> &r) {
    ips2ra::sort(r.begin(), r.end(),
      [](const Key &key) -> uint64_t { return key.bucket; });
}

inline void sort_fingerprints(std::ranges::subrange<std::vector<Key>::iterator> &r) {
    ips2ra::sort(r.begin(), r.end(),
      [](const Key &key) -> uint64_t { return key.fingerprint; });
}

template <uint64_t k>
class ThresholdBasedBumpingConsensus {
private:
    static_assert(k > 0);
    static constexpr uint64_t MAX_THRESHOLD_SIZE = 24;
    uint64_t n = 0;
    /** Fractional number of threshold bits per bin as 32.32 fixed point. Bin b of a layer uses the bits
     * [off(b), off(b+1)) of the layer's consensus stream with off(b) = floor(b * bitsFixed / 2^32),
     * so bins have either floor(bits) or ceil(bits) threshold bits. */
    uint64_t bitsFixed = 0;
    uint64_t minLength = 0;
    double effectiveThresholdBits = 0;
    using Codebooks = std::array<std::vector<uint64_t>, 2>;
    /** codebookSets[s][l] is the codebook for length minLength + l (l = 0, 1). Set 0 is for bins with
     * Pois(gamma k) keys. Small codebooks (no "bump everything" threshold) need a second set for the last layer if
     * that one is overloaded more than gamma (because it has to take the rest of the budget). */
    std::vector<Codebooks> codebookSets;
    std::vector<std::pair<uint64_t, Consensus>> layers;
    std::vector<uint8_t> layerCodebookSet;
    /** True if keys were left over after the bin budget was used up and had to be repaired into empty slots. */
    bool repaired = false;
    RepairMode repairUsed = RepairMode::Auto;
    size_t numRepaired = 0;
    double repairLowerBound = 0;
    fips::FiPS<> phf;
    mutable sux::bits::EliasFano<> gaps;
    CapacityRepair<> displace;
public:
    /**
     * Number of bins of the output range: ceil(n/k) for alpha=1, otherwise ceil(n/k)/alpha (rounded down,
     * like the output range of the benchmark framework), but never less than ceil(n/k).
     */
    static uint64_t totalBuckets(uint64_t n, double loadFactor) {
        uint64_t minimal = (n + k - 1) / k;
        if (loadFactor >= 1.0) {
            return minimal;
        }
        return std::max(minimal, static_cast<uint64_t>(double(minimal) / loadFactor));
    }

    /**
     * Number of bins of the next layer that gets curKeys keys when `remaining` bins of the budget are left.
     * The layer gets curKeys/(k*overload) bins, so overload > 1 overloads the bins and overload < 1 (only useful
     * for alpha < 1) underloads them. The layer takes the rest of the budget if that is needed to fit the keys,
     * or if the keys that cannot be placed in this layer even if all of its bins were full would overload the
     * rest of the budget more than this layer.
     */
    static uint64_t layerBuckets(uint64_t curKeys, uint64_t remaining, double overload) {
        double overloadBucketSize = k * overload;
        uint64_t curBuckets = std::max<uint64_t>(1, std::ceil(double(curKeys) / overloadBucketSize));
        if (curBuckets >= remaining) {
            return remaining;
        }
        if (curKeys > k * curBuckets
                && double(curKeys - k * curBuckets) / double(remaining - curBuckets) > overloadBucketSize) {
            return remaining;
        }
        return curBuckets;
    }

    uint64_t operator()(const std::string &key) const {
        return operator()(Hash128(key));
    }

    uint64_t operator()(const Hash128 hash) const {
        uint64_t offset = 0;
        for (uint64_t i = 0; i < layers.size(); i++) {
            auto &[cur_buckets, consensus] = layers[i];
            uint64_t b = calculateBucket(hash, i, cur_buckets);
            uint64_t off = bitOffset(b);
            uint64_t len = bitOffset(b + 1) - off;
            auto [seed, tidx] = consensus.get(off, len);
            tidx = decrypt(seed, tidx, len);
            uint64_t f = bytehamster::util::remix(hash.lo + seed + i);

            if (f < codebookSets[layerCodebookSet[i]][len - minLength][tidx]) {
                return offset + b;
            }

            offset += cur_buckets;
        }

        if (!repaired) {
            return 0; // Only reachable for keys that were not in the input set
        }
        if (repairUsed == RepairMode::Displace) {
            return displace(hash.hi ^ hash.lo);
        }
        return gaps.select(phf(hash.hi ^ hash.lo));
    }

    /** Number of keys that were placed by the repair step. */
    [[nodiscard]] size_t numRepairedKeys() const {
        return numRepaired;
    }

    /** Threshold bits per bin that were actually used (raised for alpha < 1 if too small, see constructor) */
    [[nodiscard]] double thresholdBitsUsed() const {
        return effectiveThresholdBits;
    }

    [[nodiscard]] size_t numLayers() const {
        return layers.size();
    }

    /** Which repair method was used, Auto if none */
    [[nodiscard]] RepairMode repairMethod() const {
        return repaired ? repairUsed : RepairMode::Auto;
    }

    /** Space lower bound in bits for the repair step (placing the leftover keys into the free slots) */
    [[nodiscard]] double repairLowerBoundBits() const {
        return repairLowerBound;
    }

    [[nodiscard]] size_t repairBits() const {
        if (!repaired) return 0;
        if (repairUsed == RepairMode::Displace) return displace.count_bits() - sizeof(displace) * 8;
        return phf.getBits() - sizeof(phf) * 8 + gaps.bitCount() - sizeof(gaps) * 8;
    }

    size_t count_bits() const {
        size_t s = 0;
        for (auto &[nb,c] : layers) {
            s += c.count_bits() - sizeof(c) * 8;
        }
        // Codebooks only depend on k, gamma, the threshold bits and (for the last layer) its load, so they are
        // not counted, like the lookup table of the original implementation.
        return sizeof(*this) * 8
            + layers.capacity() * sizeof(layers[0]) * 8
            + layerCodebookSet.capacity() * 8
            + 64 // load of the last layer, from which its codebooks could be recomputed
            + s
            + repairBits();
    }

private:
    uint64_t bitOffset(uint64_t bin) const {
        return uint64_t((__uint128_t(bin) * bitsFixed) >> 32);
    }

    static inline size_t calculateBucket(Hash128 key, size_t layer, size_t cur_buckets) {
        return bytehamster::util::fastrange64(bytehamster::util::remix(key.hi + layer), cur_buckets);
    }

    static inline uint64_t mask(uint64_t len) {
        return len == 0 ? 0 : (~0ul >> (64 - len));
    }

    static inline uint64_t encrypt(uint64_t key, uint64_t tidx, uint64_t len) {
        return (tidx ^ bytehamster::util::remix(key)) & mask(len);
    }

    static inline uint64_t decrypt(uint64_t key, uint64_t tidx, uint64_t len) {
        return (tidx ^ bytehamster::util::remix(key)) & mask(len);
    }

    struct LayerBuilder {
        std::vector<Key> &keys;
        std::ranges::subrange<std::vector<Key>::iterator> current;
        uint64_t cur_bucket, total_buckets, layer, offset;
        std::vector<uint64_t> &emptySlots;
        std::vector<Hash128> bumped;

        const ThresholdBasedBumpingConsensus &parent;
        const Codebooks &codebooks;
        std::array<AllowedImperfectBumping, 2> &allowedImperfectBumping;

        LayerBuilder(const ThresholdBasedBumpingConsensus &parent, const Codebooks &codebooks,
                    std::array<AllowedImperfectBumping, 2> &allowedImperfectBumping,
                    std::vector<Key> &keys, uint64_t layer, uint64_t buckets,
                    uint64_t offset, std::vector<uint64_t> &emptySlots)
                : keys(keys), current(keys.begin(), keys.begin()),
                  cur_bucket(0), total_buckets(buckets), layer(layer), offset(offset),
                  emptySlots(emptySlots), parent(parent), codebooks(codebooks),
                  allowedImperfectBumping(allowedImperfectBumping) {
        }

        uint64_t length(uint64_t bin) const {
            return parent.bitOffset(bin + 1) - parent.bitOffset(bin);
        }

        std::optional<uint64_t> advance(uint64_t seed) {
            if (cur_bucket == total_buckets) {
                return {};
            }

            auto it = current.end();
            while (it != keys.end() && it->bucket == cur_bucket) {
                it->fingerprint = bytehamster::util::remix(it->hash.lo + seed + layer);
                ++it;
            }

            current = std::ranges::subrange(current.end(), it);
            sort_fingerprints(current);

            cur_bucket++;

            return length(cur_bucket - 1);
        }

        uint64_t backtracks = 0;

        uint64_t backtrack() {
            // Too few threshold bits for the bins make the search exponential. Give up instead of hanging.
            if (++backtracks > 2000 * total_buckets + 10000000) {
                throw std::runtime_error("ThresholdBasedBumpingConsensus: too few threshold bits, search does not end");
            }
            cur_bucket--;

            if (cur_bucket == 0) {
                current = std::ranges::subrange(keys.begin(), keys.begin());
                return 0;
            }

            auto it = current.begin();
            while (it != keys.begin() && prev(it)->bucket == cur_bucket-1) {
                --it;
            }
            current = std::ranges::subrange(it, current.begin());

            uint64_t cnt = 0;
            while (!emptySlots.empty() && emptySlots.back() == offset + cur_bucket - 1) {
                emptySlots.pop_back();
                cnt++;
            }
            bumped.resize(bumped.size() - (current.size() + cnt - k));

            return length(cur_bucket - 1);
        }

        std::optional<uint64_t> find_first(uint64_t seed) {
            const auto &thresholds = codebooks[length(cur_bucket - 1) - parent.minLength];
            if (current.size() > k) {
                uint64_t fp = current[k].fingerprint;
                auto it = std::ranges::upper_bound(thresholds, fp);
                return find(seed, it - begin(thresholds));
            } else {
                return find(seed, thresholds.size());
            }
        }

        std::optional<uint64_t> find_next(uint64_t seed, uint64_t prev) {
            return find(seed, decrypt(seed, prev, length(cur_bucket - 1)));
        }

        std::optional<uint64_t> find(uint64_t seed, uint64_t prev) {
            uint64_t len = length(cur_bucket - 1);
            const auto &thresholds = codebooks[len - parent.minLength];
            auto &allowedTable = allowedImperfectBumping[len - parent.minLength];
            uint64_t goal = std::min(k, current.size());
            AllowedImperfectBumpingEntry allowed = {};
            if (current.size() < allowedTable.maxBucketSize()) {
                allowed = allowedTable.getAllowed(current.size());
            } else {
                allowed = {k + 1, 0};
            }
            for (uint64_t tidx = prev; tidx > 0;) {
                tidx--;
                auto it = std::ranges::lower_bound(current, thresholds[tidx], {},
                    [](const Key &key) { return key.fingerprint; });
                uint64_t idx = it - current.begin();
                if (idx > k) {
                    continue; // Possible for codebooks without threshold 0: this threshold keeps too many keys
                }
                uint64_t error = goal > idx ? goal - idx : 0;
                if (error > allowed.allowedBumped) {
                    break;
                }
                if (error < allowed.allowedBumped || bytehamster::util::remix(seed + tidx) < allowed.probability) {
                    for (Key & key: current | std::views::drop(idx)) {
                        bumped.push_back(key.hash);
                    }
                    for (uint64_t x = idx; x < k; x++) {
                        emptySlots.push_back(offset + cur_bucket - 1);
                    }
                    return encrypt(seed, tidx, len);
                }
            }
            return {};
        }
    };

    public:
    ThresholdBasedBumpingConsensus() : gaps({}, 0) {
    }

    /**
     * @param overload gamma: every layer gets (number of keys)/(k*gamma) bins.
     * @param thresholdBits average number of threshold bits per bin, may be fractional (bins get floor or ceil of
     *        it, as in the consensus paper). Below 2 bits, smaller codebooks are used (see threshold_codebook).
     * @param loadFactor alpha: the output range has n/(k*alpha) bins. Layers are added until all keys are placed
     *        or the bins are used up. Only keys that are left over after that are repaired into empty slots.
     * @param repairMode how leftover keys are placed into empty slots.
     */
    explicit ThresholdBasedBumpingConsensus(const std::vector<std::string> &keys, double overload,
                                            double thresholdBits, double loadFactor = 1.0,
                                            RepairMode repairMode = RepairMode::Auto)
            : ThresholdBasedBumpingConsensus(hashKeys(keys, overload, loadFactor), overload, thresholdBits,
                                             loadFactor, repairMode) {
    }

    static std::vector<Key> hashKeys(const std::vector<std::string> &keys, double overload, double loadFactor = 1.0) {
        uint64_t cur_buckets = layerBuckets(keys.size(), totalBuckets(keys.size(), loadFactor), overload);
        std::vector<Key> hashed_keys;
        hashed_keys.reserve(keys.size());
        for (const auto &key : keys) {
            Hash128 hash(key);
            hashed_keys.emplace_back(calculateBucket(hash, 0, cur_buckets), 0, hash);
        }
        return hashed_keys;
    }

    explicit ThresholdBasedBumpingConsensus(std::vector<Key> keys, double overload, double thresholdBits,
                                            double loadFactor = 1.0, RepairMode repairMode = RepairMode::Auto)
            : n(keys.size()), gaps({}, 0) {
        assert(loadFactor > 0 && loadFactor <= 1.0);
        if (thresholdBits < 0 || thresholdBits > double(MAX_THRESHOLD_SIZE - 1)) {
            throw std::invalid_argument("thresholdBits out of range");
        }
        // For alpha < 1 and fewer than 2 threshold bits per bin, the codebooks thin every bin to about Pois(mu)
        // with E[Pois(mu) | <= k] = alpha k (slightly more, so that the budget suffices), see threshold_codebook.
        double targetMu = 0;
        if (loadFactor < 0.99 && thresholdBits < 2.0) {
            std::vector<uint64_t> oneBin(k + 1, 0);
            oneBin[k] = 1;
            double mu = tpois::solveLambda(oneBin, std::min(0.999 * k, 1.01 * loadFactor * k));
            // A bin with a single threshold needs E_Y[-log2 P(success)] bits on average, which the bins with an
            // extra bit have to provide. Use at least 1.5 times that, otherwise the consensus search gets very slow.
            // Only worth it if that is cheap and the threshold is not capped (which would leave too few keys in
            // the bins). Otherwise (alpha close to 1), windows of allowed empty slots with >= 2 bits per bin are
            // better than accepting every bin with at most k keys.
            auto single = threshold_codebook(k, k * overload, 0, mu);
            double cost = small_codebook_cost(k, k * overload, single);
            if (single[0] >= 0.999 * mu / (k * overload) && cost <= 1.0) {
                targetMu = mu;
                thresholdBits = std::max(thresholdBits, 1.5 * cost + 0.01);
            }
        }
        if (thresholdBits < 2.0 && targetMu == 0) {
            // Cost-driven small codebooks: bins with 0 bits need 0.005 bits, bins with 1 bit provide 0.4 bits of
            // slack, so the average has to be clearly above 0.0125 bits per bin (less than 0.25 is slow in practice)
            thresholdBits = std::max(thresholdBits, 0.25);
        }
        effectiveThresholdBits = thresholdBits;
        bitsFixed = static_cast<uint64_t>(std::llround(std::ldexp(thresholdBits, 32)));
        minLength = bitsFixed >> 32;
        struct CodebookSet {
            Codebooks codebooks;
            std::array<AllowedImperfectBumping, 2> allowed;
        };
        auto makeSet = [&](double lambda) {
            std::array<std::vector<double>, 2> normalized;
            Codebooks cbs;
            for (uint64_t l = 0; l < 2; l++) {
                normalized[l] = threshold_codebook(k, lambda, minLength + l, targetMu);
                for (double t : normalized[l]) {
                    cbs[l].push_back(ThresholdBasedBumping::double_to_u64(t));
                }
            }
            return CodebookSet{cbs, {AllowedImperfectBumping(k, lambda / k, normalized[0]),
                                     AllowedImperfectBumping(k, lambda / k, normalized[1])}};
        };
        std::vector<CodebookSet> sets;
        sets.push_back(makeSet(k * overload));
        codebookSets.push_back(sets[0].codebooks);

        uint64_t total_buckets = totalBuckets(n, loadFactor);

#ifdef STATS
        total_keys += n;
#endif

        uint64_t offset = 0;
        std::vector<uint64_t> emptySlots;
        std::vector<Hash128> bumped;
        for (uint64_t layer = 0; offset != total_buckets; layer++) {
            size_t cur_keys = layer > 0 ? bumped.size() : keys.size();
            if (cur_keys == 0) {
                break; // All keys placed, the rest of the budget stays empty
            }
            uint64_t cur_buckets = layerBuckets(cur_keys, total_buckets - offset, overload);

            if (layer > 0) {
                keys.clear();
                keys.reserve(bumped.size());
                for (const Hash128 &key : bumped) {
                    keys.emplace_back(calculateBucket(key, layer, cur_buckets), 0, key);
                }
                bumped.clear();
            }

            sort_buckets(keys);

            uint8_t set = 0;
            double layerLambda = double(cur_keys) / double(cur_buckets);
            if (minLength < 2 && layerLambda > 1.01 * k * overload) {
                // Overloaded last layer, small codebooks need thresholds for its actual load
                sets.push_back(makeSet(layerLambda));
                codebookSets.push_back(sets.back().codebooks);
                set = sets.size() - 1;
            }
            LayerBuilder builder(*this, sets[set].codebooks, sets[set].allowed, keys, layer, cur_buckets, offset,
                                 emptySlots);
            Consensus consensus(builder);
            layers.emplace_back(cur_buckets, std::move(consensus));
            layerCodebookSet.push_back(set);
            std::swap(bumped, builder.bumped);

            offset += cur_buckets;
        }
        layers.shrink_to_fit();
        layerCodebookSet.shrink_to_fit();

#ifdef STATS
        bumped_keys += bumped.size();
#endif

        if (bumped.empty()) {
            return; // No repair needed
        }
        repaired = true;
        numRepaired = bumped.size();

        std::vector<uint64_t> fallbackKeys;
        fallbackKeys.reserve(bumped.size());
        for (Hash128 key : bumped) {
            fallbackKeys.push_back(key.hi ^ key.lo);
        }
        std::vector<uint32_t> loads(total_buckets, 0);
        for (uint64_t b = 0; b < offset; b++) loads[b] = k;
        for (uint64_t b : emptySlots) loads[b]--;
        repairLowerBound = CapacityRepair<>::lowerBoundBits(loads, k, fallbackKeys.size());

        repairUsed = repairMode;
        if (repairUsed == RepairMode::Auto) {
            repairUsed = CapacityRepair<>::isSuitable(fallbackKeys.size(), emptySlots.size())
                    ? RepairMode::Displace : RepairMode::FipsEliasFano;
        }
        if (repairUsed == RepairMode::Displace) {
            displace = CapacityRepair<>(fallbackKeys, std::move(loads), k);
            return;
        }

        phf = fips::FiPS<>(fallbackKeys);
        std::vector<uint64_t> actual_spots;
        for (uint64_t key : fallbackKeys) {
            uint64_t h = phf(key);
            if (h >= actual_spots.size()) {
                actual_spots.resize(h + 1);
            }
            actual_spots[h] = 1;
        }
        auto it = emptySlots.begin();
        for (uint64_t &x : actual_spots) {
            uint64_t v = *it;
            if (x) {
                ++it;
            }
            x = v;
        }
        gaps = sux::bits::EliasFano<>(actual_spots, total_buckets);
    }
};

}
