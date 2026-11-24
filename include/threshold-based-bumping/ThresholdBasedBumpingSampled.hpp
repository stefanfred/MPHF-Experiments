#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "ThresholdBasedBumpingConsensus.hpp"

/**
 * Threshold-based bumping where the threshold of a bin is not an index into a codebook, but sampled from a
 * distribution g by hashing the consensus seed together with the bin's own (fractional number of) bits.
 * So every bin, also one with 0 bits, gets a new threshold (and new fingerprints) whenever an earlier bin changes,
 * and the number of bits of a bin does not restrict which threshold values it can use.
 *
 * With Y ~ Pois(lambda) keys in a bin (fixed by the hash of the layer), a seed is valid if the number of kept keys
 * (fingerprint below the threshold) is in a window [a(Y), k]. The consensus search then needs about
 * E_Y[-log2 M_Y] bits per bin, M_Y = sum_T g(T) P[Bin(Y, T) in [a(Y), k]], and a bin keeps
 * E_Y[E[kept | valid]] keys on average. g and a(Y) are chosen by minimizing cost - nu * fill
 * (exponentiated gradient over g on a grid of 64 thresholds, best window for each Y), with nu such that the fill
 * is alpha*k (slightly more, so that the budget suffices). This is the model of the analysis; for small alpha it
 * puts all mass on one threshold mu/lambda and accepts every bin with at most k kept keys, for larger alpha it
 * uses windows and a spread of thresholds.
 */
namespace kphf::ThresholdBasedBumpingSampled {

using kphf::ThresholdBasedBumpingConsensus::Key;
using kphf::ThresholdBasedBumpingConsensus::Consensus;
using kphf::ThresholdBasedBumpingConsensus::RepairMode;
using kphf::ThresholdBasedBumpingConsensus::repairModeName;
using kphf::ThresholdBasedBumpingConsensus::sort_buckets;
using kphf::ThresholdBasedBumpingConsensus::sort_fingerprints;

struct ThresholdDistribution {
    static constexpr size_t GRID = 64;
    static constexpr size_t TABLE_BITS = 12;
    static constexpr double MAX_BIN_COST = 4.0;
    /** Quantile table: threshold (as fraction of 2^64) for the top TABLE_BITS bits of a uniform hash */
    std::vector<uint64_t> table;
    /** Smallest number of kept keys that is accepted for a bin of size Y (index), 0 for larger bins */
    std::vector<uint32_t> minKept;
    double costBits = 0;
    double fill = 0;
    double lambda = 0;

    uint32_t minKeptFor(size_t binSize) const {
        return binSize < minKept.size() ? minKept[binSize] : 0;
    }

    /**
     * @param k bin capacity
     * @param lambda expected number of keys per bin (the layer's load)
     * @param targetFill expected number of kept keys per bin that the layers should reach
     * @param maxCost if reaching targetFill costs more bits per bin, take the largest fill with at most this cost
     */
    static ThresholdDistribution optimize(uint64_t k, double lambda, double targetFill,
                                          double maxCost = std::numeric_limits<double>::infinity()) {
        ThresholdDistribution d;
        d.lambda = lambda;
        const size_t J = GRID;
        const size_t K1 = k + 1;
        const size_t maxY = size_t(lambda + 12 * std::sqrt(lambda) + 20);
        std::vector<double> pY(maxY + 1);
        {
            double p = std::exp(-lambda), sum = 0;
            for (size_t y = 0; y <= maxY; y++) { pY[y] = p; sum += p; p *= lambda / double(y + 1); }
            for (double &x : pY) x /= sum;
        }
        std::vector<double> T(J);
        for (size_t j = 0; j < J; j++) T[j] = double(j + 1) / double(J);
        // Suffix sums over the kept count c >= a of P[Bin(Y, T_j) = c] (only c <= k is valid) and of c times that
        std::vector<double> mass((maxY + 1) * J * (K1 + 1), 0.0), wsum((maxY + 1) * J * (K1 + 1), 0.0);
        auto at = [&](size_t y, size_t j, size_t a) { return (y * J + j) * (K1 + 1) + a; };
        std::vector<double> pmf(K1);
        for (size_t y = 0; y <= maxY; y++) {
            for (size_t j = 0; j < J; j++) {
                std::fill(pmf.begin(), pmf.end(), 0.0);
                if (T[j] >= 1.0) {
                    if (y <= k) pmf[y] = 1.0;
                } else {
                    double lt = std::log(T[j]), l1t = std::log1p(-T[j]);
                    for (size_t c = 0; c <= std::min<size_t>(y, k); c++) {
                        pmf[c] = std::exp(std::lgamma(y + 1.0) - std::lgamma(c + 1.0) - std::lgamma(double(y - c) + 1.0)
                                          + double(c) * lt + double(y - c) * l1t);
                    }
                }
                for (size_t a = K1; a-- > 0;) {
                    mass[at(y, j, a)] = mass[at(y, j, a + 1)] + pmf[a];
                    wsum[at(y, j, a)] = wsum[at(y, j, a + 1)] + double(a) * pmf[a];
                }
            }
        }

        struct Result { double cost, fill; std::vector<double> g; std::vector<uint32_t> a; };
        auto solve = [&](double nu) {
            std::vector<double> g(J, 1.0 / J), grad(J);
            std::vector<uint32_t> best(maxY + 1, 0);
            double cost = 0, fill = 0;
            for (int it = 0; it < 80; it++) {
                std::fill(grad.begin(), grad.end(), 0.0);
                cost = 0; fill = 0;
                for (size_t y = 0; y <= maxY; y++) {
                    // Best window [a, k] for this bin size. A bin may cost at most MAX_BIN_COST bits (if possible),
                    // otherwise single rare large bins need very deep backtracking in the consensus search.
                    double bestObj = std::numeric_limits<double>::infinity();
                    double bestM = 0, bestW = 0;
                    bool bestCapped = false;
                    for (size_t a = 0; a <= std::min<size_t>(y, k); a++) {
                        double M = 0, W = 0;
                        for (size_t j = 0; j < J; j++) { M += g[j] * mass[at(y, j, a)]; W += g[j] * wsum[at(y, j, a)]; }
                        if (M <= 1e-300) continue;
                        bool capped = -std::log2(M) <= MAX_BIN_COST;
                        double obj = -std::log2(M) - nu * W / M;
                        if ((capped && !bestCapped) || (capped == bestCapped && obj < bestObj)) {
                            bestObj = obj; best[y] = a; bestM = M; bestW = W; bestCapped = capped;
                        }
                    }
                    if (bestM <= 0) continue;
                    cost += pY[y] * -std::log2(bestM);
                    fill += pY[y] * bestW / bestM;
                    for (size_t j = 0; j < J; j++) {
                        double m = mass[at(y, j, best[y])], w = wsum[at(y, j, best[y])];
                        grad[j] += pY[y] * (m / (bestM * std::log(2.0)) + nu * (w * bestM - bestW * m) / (bestM * bestM));
                    }
                }
                double gmax = *std::max_element(grad.begin(), grad.end());
                double gabs = 1e-12;
                for (double x : grad) gabs = std::max(gabs, std::abs(x));
                double sum = 0;
                for (size_t j = 0; j < J; j++) { g[j] *= std::exp(2.5 * (grad[j] - gmax) / gabs); sum += g[j]; }
                for (double &x : g) x /= sum;
            }
            return Result{cost, fill, g, best};
        };

        // Smallest nu (cheapest) that reaches the target fill, but at most maxCost bits per bin
        double lo = std::log(1e-4), hi = std::log(200.0);
        Result res = solve(std::exp(hi));
        if (res.fill >= targetFill || res.cost > maxCost) {
            Result fallback = solve(std::exp(lo));
            for (int it = 0; it < 12; it++) {
                double mid = 0.5 * (lo + hi);
                Result r = solve(std::exp(mid));
                if (r.fill >= targetFill || r.cost > maxCost) { hi = mid; res = std::move(r); }
                else { lo = mid; fallback = std::move(r); }
            }
            if (res.cost > maxCost) res = std::move(fallback);
        }
        // Every threshold of the grid keeps a small probability, so that rare bin sizes that only one grid point
        // fixes are not lost by the quantization of the table (the search would never end for them)
        {
            double floor = 2.0 / double(1ul << TABLE_BITS), sum = 0;
            for (double &x : res.g) { x = std::max(x, floor); sum += x; }
            for (double &x : res.g) x /= sum;
        }
        d.costBits = res.cost;
        d.fill = res.fill;
        d.minKept = res.a;
        d.table.resize(1ul << TABLE_BITS);
        double cumulative = 0;
        size_t j = 0;
        for (size_t q = 0; q < d.table.size(); q++) {
            double u = (q + 0.5) / double(d.table.size());
            while (j + 1 < J && cumulative + res.g[j] < u) { cumulative += res.g[j]; j++; }
            d.table[q] = T[j] >= 1.0 ? ~0ul : uint64_t(std::ldexp(T[j], 64));
        }
        return d;
    }
};

/**
 * Single fixed threshold T per layer (the seed only changes the fingerprints): for every T on a grid, choose the
 * windows a(Y) for a fill of targetFill (bisection on the multiplier, at most MAX_BIN_COST bits per bin size if
 * possible) and take the T with the smallest cost. In the model, this is as good as an optimized distribution of
 * thresholds (the per-size windows adapt to the bin size instead), and the 2^L values of a bin are independent
 * trials because they change the fingerprints.
 */
inline ThresholdDistribution optimizeFixedThreshold(uint64_t k, double lambda, double targetFill,
                                                    double maxCost = std::numeric_limits<double>::infinity()) {
    const size_t K1 = k + 1;
    const size_t maxY = size_t(lambda + 12 * std::sqrt(lambda) + 20);
    std::vector<double> pY(maxY + 1);
    {
        double p = std::exp(-lambda), sum = 0;
        for (size_t y = 0; y <= maxY; y++) { pY[y] = p; sum += p; p *= lambda / double(y + 1); }
        for (double &x : pY) x /= sum;
    }
    std::vector<double> mass((maxY + 1) * (K1 + 1)), wsum((maxY + 1) * (K1 + 1)), pmf(K1);
    struct Result { double cost, fill; std::vector<uint32_t> a; };
    auto solveT = [&](double T, double nu) {
        Result r{0, 0, std::vector<uint32_t>(maxY + 1, 0)};
        for (size_t y = 0; y <= maxY; y++) {
            double bestObj = std::numeric_limits<double>::infinity(), bestM = 0, bestW = 0;
            bool bestCapped = false;
            for (size_t a = 0; a <= std::min<size_t>(y, k); a++) {
                double M = mass[y * (K1 + 1) + a], W = wsum[y * (K1 + 1) + a];
                if (M <= 1e-300) continue;
                bool capped = -std::log2(M) <= ThresholdDistribution::MAX_BIN_COST;
                double obj = -std::log2(M) - nu * W / M;
                if ((capped && !bestCapped) || (capped == bestCapped && obj < bestObj)) {
                    bestObj = obj; bestM = M; bestW = W; bestCapped = capped; r.a[y] = a;
                }
            }
            if (bestM <= 0) continue;
            r.cost += pY[y] * -std::log2(bestM);
            r.fill += pY[y] * bestW / bestM;
        }
        return r;
    };
    double bestCost = std::numeric_limits<double>::infinity(), bestFill = -1, bestT = 1;
    Result best{};
    for (int step = 1; step <= 200; step++) {
        double T = step / 200.0;
        for (size_t y = 0; y <= maxY; y++) {
            std::fill(pmf.begin(), pmf.end(), 0.0);
            if (T >= 1.0) {
                if (y <= k) pmf[y] = 1.0;
            } else {
                for (size_t c = 0; c <= std::min<size_t>(y, k); c++) {
                    pmf[c] = std::exp(std::lgamma(y + 1.0) - std::lgamma(c + 1.0) - std::lgamma(double(y - c) + 1.0)
                                      + double(c) * std::log(T) + double(y - c) * std::log1p(-T));
                }
            }
            mass[y * (K1 + 1) + K1] = 0; wsum[y * (K1 + 1) + K1] = 0;
            for (size_t a = K1; a-- > 0;) {
                mass[y * (K1 + 1) + a] = mass[y * (K1 + 1) + a + 1] + pmf[a];
                wsum[y * (K1 + 1) + a] = wsum[y * (K1 + 1) + a + 1] + double(a) * pmf[a];
            }
        }
        double lo = std::log(1e-4), hi = std::log(200.0);
        Result r = solveT(T, std::exp(hi));
        if (r.fill >= targetFill && r.cost <= maxCost) {
            for (int it = 0; it < 30; it++) {
                double mid = 0.5 * (lo + hi);
                Result r2 = solveT(T, std::exp(mid));
                if (r2.fill >= targetFill) { hi = mid; r = std::move(r2); } else lo = mid;
            }
            if (bestFill < targetFill || r.cost < bestCost) { bestCost = r.cost; bestFill = r.fill; bestT = T; best = r; }
        } else if (bestFill < targetFill) {
            // Target not reachable (yet): keep the largest fill within maxCost
            for (int it = 0; it < 30 && r.cost > maxCost; it++) {
                double mid = 0.5 * (lo + hi);
                r = solveT(T, std::exp(mid));
                hi = mid;
            }
            if (r.cost <= maxCost && r.fill > bestFill) { bestCost = r.cost; bestFill = r.fill; bestT = T; best = r; }
        }
    }
    ThresholdDistribution d;
    d.lambda = lambda;
    d.costBits = bestCost;
    d.fill = bestFill;
    d.minKept = best.a;
    d.table.assign(1, bestT >= 1.0 ? ~0ul : uint64_t(std::ldexp(bestT, 64)));
    return d;
}

template <uint64_t k, bool FixedThreshold = false>
class ThresholdBasedBumpingSampled {
    static_assert(k > 0 && k <= 128, "the distribution optimizer is O(k) per bin size and threshold");
    uint64_t n = 0;
    uint64_t bitsFixed = 0;
    double effectiveThresholdBits = 0;
    double modelCostBits = 0;
    std::vector<std::vector<uint64_t>> tables; // threshold quantile table of each distribution
    std::vector<std::pair<uint64_t, Consensus>> layers;
    std::vector<uint8_t> layerTable;
    bool repaired = false;
    RepairMode repairUsed = RepairMode::Auto;
    size_t numRepaired = 0;
    fips::FiPS<> phf;
    mutable sux::bits::EliasFano<> gaps;
    CapacityRepair<> displace;

    static constexpr uint64_t T_MIX = 0x9E3779B97F4A7C15ul;
    static constexpr uint64_t LAYER_MIX = 0xD1B54A32D192ED03ul;
    static constexpr uint64_t BIN_MIX = 0x8CB92BA72F3D8DD7ul;
    static constexpr uint64_t FP_MIX = 0xA0761D6478BD642Ful;

    /** Fingerprint of a key. With a fixed threshold, the bin's own value changes the fingerprints. */
    static uint64_t fingerprint(uint64_t keyLo, uint64_t seed, uint64_t value, uint64_t layer) {
        if constexpr (FixedThreshold) {
            return bytehamster::util::remix(keyLo + seed + layer + (value + 1) * FP_MIX);
        } else {
            (void) value;
            return bytehamster::util::remix(keyLo + seed + layer);
        }
    }

    static ThresholdDistribution makeDistribution(uint64_t k_, double lambda, double targetFill, double maxCost) {
        if constexpr (FixedThreshold) {
            return optimizeFixedThreshold(k_, lambda, targetFill, maxCost);
        } else {
            return ThresholdDistribution::optimize(k_, lambda, targetFill, maxCost);
        }
    }

    uint64_t bitOffset(uint64_t bin) const {
        return uint64_t((__uint128_t(bin) * bitsFixed) >> 32);
    }

    static size_t calculateBucket(Hash128 key, size_t layer, size_t curBuckets) {
        return bytehamster::util::fastrange64(bytehamster::util::remix(key.hi + layer), curBuckets);
    }

    /** Bins without bits share the consensus seed with the bin before, so the bin index is part of the hash */
    static uint64_t thresholdHash(uint64_t seed, uint64_t value, uint64_t bin, uint64_t layer) {
        return bytehamster::util::remix(seed + (value + 1) * T_MIX + layer * LAYER_MIX + bin * BIN_MIX);
    }

    struct LayerBuilder {
        std::vector<Key> &keys;
        std::ranges::subrange<std::vector<Key>::iterator> current;
        uint64_t cur_bucket, total_buckets, layer, offset;
        std::vector<uint64_t> &emptySlots;
        std::vector<Hash128> bumped;
        const ThresholdBasedBumpingSampled &parent;
        const ThresholdDistribution &dist;
        uint64_t backtracks = 0;

        LayerBuilder(const ThresholdBasedBumpingSampled &parent, const ThresholdDistribution &dist,
                     std::vector<Key> &keys, uint64_t layer, uint64_t buckets, uint64_t offset,
                     std::vector<uint64_t> &emptySlots)
                : keys(keys), current(keys.begin(), keys.begin()), cur_bucket(0), total_buckets(buckets),
                  layer(layer), offset(offset), emptySlots(emptySlots), parent(parent), dist(dist) {
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
                it->fingerprint = fingerprint(it->hash.lo, seed, 0, layer);
                ++it;
            }
            current = std::ranges::subrange(current.end(), it);
            if constexpr (!FixedThreshold) {
                sort_fingerprints(current);
            }
            cur_bucket++;
            return length(cur_bucket - 1);
        }

        uint64_t backtrack() {
            if (++backtracks > 300 * total_buckets + 1000000) {
                throw std::runtime_error("ThresholdBasedBumpingSampled: too few threshold bits, search does not end");
            }
            cur_bucket--;
            if (cur_bucket == 0) {
                current = std::ranges::subrange(keys.begin(), keys.begin());
                return 0;
            }
            auto it = current.begin();
            while (it != keys.begin() && prev(it)->bucket == cur_bucket - 1) {
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
            return find(seed, 0);
        }

        std::optional<uint64_t> find_next(uint64_t seed, uint64_t prev) {
            return find(seed, prev + 1);
        }

        std::optional<uint64_t> find(uint64_t seed, uint64_t start) {
            uint64_t len = length(cur_bucket - 1);
            uint64_t values = 1ul << len;
            uint32_t minKept = std::min<uint32_t>(dist.minKeptFor(current.size()), current.size());
            if constexpr (FixedThreshold) {
                const uint64_t t = dist.table[0];
                for (uint64_t v = start; v < values; v++) {
                    uint64_t kept = 0;
                    for (Key &key : current) {
                        key.fingerprint = fingerprint(key.hash.lo, seed, v, layer);
                        kept += key.fingerprint < t;
                    }
                    if (kept > k || kept < minKept) continue;
                    for (Key &key : current) {
                        if (key.fingerprint >= t) bumped.push_back(key.hash);
                    }
                    for (uint64_t x = kept; x < k; x++) emptySlots.push_back(offset + cur_bucket - 1);
                    return v;
                }
                return {};
            } else {
                for (uint64_t v = start; v < values; v++) {
                    uint64_t t = dist.table[thresholdHash(seed, v, cur_bucket - 1, layer)
                                            >> (64 - ThresholdDistribution::TABLE_BITS)];
                    auto it = std::ranges::lower_bound(current, t, {}, [](const Key &key) { return key.fingerprint; });
                    uint64_t kept = it - current.begin();
                    if (kept > k || kept < minKept) continue;
                    for (Key &key : current | std::views::drop(kept)) bumped.push_back(key.hash);
                    for (uint64_t x = kept; x < k; x++) emptySlots.push_back(offset + cur_bucket - 1);
                    return v;
                }
                return {};
            }
        }
    };

public:
    static uint64_t totalBuckets(uint64_t n, double loadFactor) {
        return kphf::ThresholdBasedBumpingConsensus::ThresholdBasedBumpingConsensus<k>::totalBuckets(n, loadFactor);
    }

    static uint64_t layerBuckets(uint64_t curKeys, uint64_t remaining, double overload) {
        return kphf::ThresholdBasedBumpingConsensus::ThresholdBasedBumpingConsensus<k>::layerBuckets(
                curKeys, remaining, overload);
    }

    ThresholdBasedBumpingSampled() : gaps({}, 0) {
    }

    /**
     * @param overload gamma: every layer gets (number of keys)/(k*gamma) bins
     * @param thresholdBits minimum average number of bits per bin. The bits actually used are at least
     *        slack * (model cost) + 0.02, see thresholdBitsUsed.
     * @param loadFactor alpha, the output range has ceil(n/k)/alpha bins
     * @param slack factor on the model cost, larger makes the consensus search faster
     */
    ThresholdBasedBumpingSampled(const std::vector<std::string> &strings, double overload, double thresholdBits,
                                 double loadFactor = 1.0, RepairMode repairMode = RepairMode::Auto,
                                 double slack = 1.3)
            : n(strings.size()), gaps({}, 0) {
        double targetFill = std::min(0.999 * k, 1.01 * loadFactor * k);
        ThresholdDistribution first = makeDistribution(k, k * overload, targetFill,
                                                       std::numeric_limits<double>::infinity());
        // The consensus search needs some slack over the model cost. If it does not finish within a backtracking
        // budget, retry with more bits.
        for (int attempt = 0;; attempt++) {
            try {
                build(strings, overload, thresholdBits, loadFactor, repairMode, slack, targetFill, first);
                return;
            } catch (const std::runtime_error &e) {
                if (attempt >= 6) throw;
                slack *= 1.25;
                thresholdBits = std::max(thresholdBits, 1.25 * effectiveThresholdBits);
                layers.clear();
                layerTable.clear();
                tables.clear();
            }
        }
    }

private:
    void build(const std::vector<std::string> &strings, double overload, double thresholdBits, double loadFactor,
               RepairMode repairMode, double slack, double targetFill, const ThresholdDistribution &first) {
        uint64_t total_buckets = totalBuckets(n, loadFactor);
        std::vector<Key> keys;
        {
            uint64_t cur = layerBuckets(n, total_buckets, overload);
            keys.reserve(n);
            for (const auto &s : strings) {
                Hash128 hash(s);
                keys.emplace_back(calculateBucket(hash, 0, cur), 0, hash);
            }
        }
        std::vector<ThresholdDistribution> dists;
        dists.push_back(first);
        modelCostBits = dists[0].costBits;
        effectiveThresholdBits = std::max(thresholdBits, slack * modelCostBits + 0.02);
        bitsFixed = static_cast<uint64_t>(std::llround(std::ldexp(effectiveThresholdBits, 32)));
        if ((bitsFixed >> 32) >= 40) {
            throw std::invalid_argument("too many threshold bits");
        }
        tables.push_back(dists[0].table);

        uint64_t offset = 0;
        std::vector<uint64_t> emptySlots;
        std::vector<Hash128> bumped;
        for (uint64_t layer = 0; offset != total_buckets; layer++) {
            size_t cur_keys = layer > 0 ? bumped.size() : keys.size();
            if (cur_keys == 0) break;
            uint64_t cur_buckets = layerBuckets(cur_keys, total_buckets - offset, overload);
            if (layer > 0) {
                keys.clear();
                keys.reserve(bumped.size());
                for (const Hash128 &key : bumped) keys.emplace_back(calculateBucket(key, layer, cur_buckets), 0, key);
                bumped.clear();
            }
            sort_buckets(keys);
            uint8_t d = 0;
            double layerLambda = double(cur_keys) / double(cur_buckets);
            if (layerLambda > 1.01 * k * overload && dists.size() < 255) {
                // Overloaded last layer (takes the rest of the budget): distribution for its actual load
                dists.push_back(makeDistribution(k, layerLambda, targetFill, (effectiveThresholdBits - 0.02) / slack));
                tables.push_back(dists.back().table);
                d = dists.size() - 1;
            }
            LayerBuilder builder(*this, dists[d], keys, layer, cur_buckets, offset, emptySlots);
            Consensus consensus(builder);
            layers.emplace_back(cur_buckets, std::move(consensus));
            layerTable.push_back(d);
            std::swap(bumped, builder.bumped);
            offset += cur_buckets;
        }
        layers.shrink_to_fit();
        layerTable.shrink_to_fit();

        if (bumped.empty()) return;
        repaired = true;
        numRepaired = bumped.size();
        std::vector<uint64_t> fallbackKeys;
        fallbackKeys.reserve(bumped.size());
        for (Hash128 key : bumped) fallbackKeys.push_back(key.hi ^ key.lo);
        repairUsed = repairMode;
        if (repairUsed == RepairMode::Auto) {
            repairUsed = CapacityRepair<>::isSuitable(fallbackKeys.size(), emptySlots.size())
                    ? RepairMode::Displace : RepairMode::FipsEliasFano;
        }
        if (repairUsed == RepairMode::Displace) {
            std::vector<uint32_t> loads(total_buckets, 0);
            for (uint64_t b = 0; b < offset; b++) loads[b] = k;
            for (uint64_t b : emptySlots) loads[b]--;
            displace = CapacityRepair<>(fallbackKeys, std::move(loads), k);
            return;
        }
        phf = fips::FiPS<>(fallbackKeys);
        std::vector<uint64_t> actual_spots;
        for (uint64_t key : fallbackKeys) {
            uint64_t h = phf(key);
            if (h >= actual_spots.size()) actual_spots.resize(h + 1);
            actual_spots[h] = 1;
        }
        auto it = emptySlots.begin();
        for (uint64_t &x : actual_spots) {
            uint64_t v = *it;
            if (x) ++it;
            x = v;
        }
        gaps = sux::bits::EliasFano<>(actual_spots, total_buckets);
    }

public:
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
            auto [seed, value] = consensus.get(off, len);
            uint64_t t;
            if constexpr (FixedThreshold) {
                t = tables[layerTable[i]][0];
            } else {
                t = tables[layerTable[i]][thresholdHash(seed, value, b, i) >> (64 - ThresholdDistribution::TABLE_BITS)];
            }
            if (fingerprint(hash.lo, seed, value, i) < t) {
                return offset + b;
            }
            offset += cur_buckets;
        }
        if (!repaired) return 0;
        if (repairUsed == RepairMode::Displace) return displace(hash.hi ^ hash.lo);
        return gaps.select(phf(hash.hi ^ hash.lo));
    }

    [[nodiscard]] size_t repairBits() const {
        if (!repaired) return 0;
        if (repairUsed == RepairMode::Displace) return displace.count_bits() - sizeof(displace) * 8;
        return phf.getBits() - sizeof(phf) * 8 + gaps.bitCount() - sizeof(gaps) * 8;
    }

    size_t count_bits() const {
        size_t s = 0;
        for (auto &[nb, c] : layers) s += c.count_bits() - sizeof(c) * 8;
        // The quantile tables only depend on k, gamma, alpha (and the load of the last layer), not counted
        return sizeof(*this) * 8 + layers.capacity() * sizeof(layers[0]) * 8 + layerTable.capacity() * 8 + 64
               + s + repairBits();
    }

    [[nodiscard]] double thresholdBitsUsed() const { return effectiveThresholdBits; }
    [[nodiscard]] double modelCost() const { return modelCostBits; }
    [[nodiscard]] size_t numLayers() const { return layers.size(); }
    [[nodiscard]] size_t numRepairedKeys() const { return numRepaired; }
    [[nodiscard]] RepairMode repairMethod() const { return repaired ? repairUsed : RepairMode::Auto; }
};

}
