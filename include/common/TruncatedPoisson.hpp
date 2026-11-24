#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

/**
 * Helpers for the truncated Poisson model of bin loads that gives the space lower bounds of k-perfect hashing
 * and of filling bins with given free capacities.
 *
 * S_e(lambda) = sum_{i<=e} lambda^i / i!. For a random function of n keys into B bins, the probability that bin j
 * gets at most e_j keys is (saddle point / exponential tilting, up to polynomial factors)
 *     -ln P = max_lambda [ n ln(lambda B / n) + n - sum_j ln S_{e_j}(lambda) ],
 * where the maximum is attained for the lambda with sum_j E[Pois(lambda) | <= e_j] = n.
 */
namespace kphf::tpois {

inline double logAddExp(double a, double b) {
    if (a < b) std::swap(a, b);
    if (b == -std::numeric_limits<double>::infinity()) return a;
    return a + std::log1p(std::exp(b - a));
}

/** out[e] = ln S_e(lambda) for e = 0..maxE */
inline void logPartialSums(double lambda, size_t maxE, std::vector<double> &out) {
    out.resize(maxE + 1);
    double logLambda = std::log(lambda);
    out[0] = 0.0;
    for (size_t e = 1; e <= maxE; e++) {
        double term = double(e) * logLambda - std::lgamma(double(e) + 1.0);
        out[e] = logAddExp(out[e - 1], term);
    }
}

/** Expected number of keys in bins with the given histogram of free capacities (hist[e] = #bins with e free slots) */
inline double expectedKeys(const std::vector<uint64_t> &hist, double lambda, std::vector<double> &scratch) {
    if (hist.size() <= 1) return 0;
    logPartialSums(lambda, hist.size() - 1, scratch);
    double sum = 0;
    for (size_t e = 1; e < hist.size(); e++) {
        if (hist[e] == 0) continue;
        // E[Pois(lambda) | <= e] = lambda S_{e-1} / S_e
        sum += double(hist[e]) * lambda * std::exp(scratch[e - 1] - scratch[e]);
    }
    return sum;
}

/** lambda such that sum_e hist[e] E[Pois(lambda) | <= e] = n. Requires n < sum_e e * hist[e]. */
inline double solveLambda(const std::vector<uint64_t> &hist, double n) {
    std::vector<double> scratch;
    double lo = std::log(1e-12), hi = std::log(1e12);
    for (int it = 0; it < 100; it++) {
        double mid = 0.5 * (lo + hi);
        if (expectedKeys(hist, std::exp(mid), scratch) < n) lo = mid;
        else hi = mid;
    }
    return std::exp(0.5 * (lo + hi));
}

/**
 * Lower bound in bits (brute force / counting) for mapping n keys into the bins described by hist (number of bins
 * with e free slots), such that no bin exceeds its free capacity. The query does not know the free capacities.
 */
inline double fillLowerBoundBits(const std::vector<uint64_t> &hist, uint64_t n) {
    if (n == 0) return 0;
    double bins = 0, free = 0;
    for (size_t e = 0; e < hist.size(); e++) {
        bins += double(hist[e]);
        free += double(e) * double(hist[e]);
    }
    double nd = double(n);
    if (nd > free + 1e-9) return std::numeric_limits<double>::infinity();
    double nats;
    if (nd > free - 0.5) {
        // Every free slot has to be used: -ln P = n ln(B/n) + n + sum_j ln e_j! (multinomial, Stirling for n!)
        nats = nd * std::log(bins / nd) + nd;
        for (size_t e = 2; e < hist.size(); e++) nats += double(hist[e]) * std::lgamma(double(e) + 1.0);
    } else {
        double lambda = solveLambda(hist, nd);
        std::vector<double> lnS;
        logPartialSums(lambda, hist.size() - 1, lnS);
        nats = nd * std::log(lambda * bins / nd) + nd;
        for (size_t e = 1; e < hist.size(); e++) nats -= double(hist[e]) * lnS[e];
    }
    return std::max(0.0, nats / std::log(2.0));
}

}
