#pragma once

#include <cstddef>
#include <utility>

/**
 * Values of k that are compiled into the benchmark for contenders that need k at compile time.
 * 4, 8, 16 cover SIMD widths / cache lines of 64-bit and 32-bit keys, the others are for larger buckets (e.g. disk pages).
 */
inline constexpr size_t compiledKs[] = {2, 4, 8, 10, 16, 100, 1000};

constexpr bool isCompiledK(size_t k) {
    for (size_t c : compiledKs) {
        if (c == k) return true;
    }
    return false;
}

/** Calls Runner<k>()(args...) for the given runtime k. Returns false if k was not compiled in. */
template <template <size_t _> class Runner, typename... Args>
bool dispatchK(size_t k, Args&&... args) {
    switch (k) {
        case 2:    Runner<2>()(std::forward<Args>(args)...); return true;
        case 4:    Runner<4>()(std::forward<Args>(args)...); return true;
        case 8:    Runner<8>()(std::forward<Args>(args)...); return true;
        case 10:   Runner<10>()(std::forward<Args>(args)...); return true;
        case 16:   Runner<16>()(std::forward<Args>(args)...); return true;
        case 100:  Runner<100>()(std::forward<Args>(args)...); return true;
        case 1000: Runner<1000>()(std::forward<Args>(args)...); return true;
        default:   return false;
    }
}
