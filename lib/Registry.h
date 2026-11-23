#pragma once

#include <cstddef>
#include <cmath>
#include <vector>

/** A (k, load factor) setting plus the benchmark knobs that some runners understand. */
struct RunConfig {
    size_t N = 5e6;
    /** Number of keys per output position (k=1: classic perfect hash function). */
    size_t k = 1;
    /** alpha = N / (k * M). alpha=1 means (k-)minimal, alpha<1 leaves space in the output range. */
    double alpha = 1.0;
    bool minimalOnly = false;
    bool sichashOnlyPartial = false;

    bool isMinimal() const { return alpha > 1.0 - 1e-9; }
};

/**
 * One benchmarked construction method. Every entry decides on its own if it can be run for a given (k, alpha)
 * so that the driver can sweep over a grid of settings and silently skip the combinations that do not exist.
 */
struct ContenderSpec {
    const char *flag;
    const char *description;
    bool (*supports)(size_t k, double alpha);
    void (*run)(const RunConfig &config);
};

const std::vector<ContenderSpec> &allContenders();

namespace support {
    inline bool isOne(double alpha) { return alpha > 1.0 - 1e-9 && alpha < 1.0 + 1e-9; }
    inline bool inRange(double alpha, double lo, double hi) { return alpha >= lo - 1e-9 && alpha <= hi + 1e-9; }
}
