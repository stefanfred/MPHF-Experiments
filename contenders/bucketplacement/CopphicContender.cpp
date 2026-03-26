#include "CopphicContender.h"

template<bool min_max, typename bucket_t, size_t padding, typename bucketer>
void dispatchPaddingBits(size_t N) {
    if constexpr (padding > 1) {
        dispatchPaddingBits<min_max, bucket_t, padding-2, bucketer>(N);
    }
    std::array<double, 6> space_vals = {1.64, 1.72, 1.8, 1.9, 2.1, 2.5};
    for (auto s: space_vals) {
        std::array<size_t, 3> exp_part_size_vals = {1 << 14, 1 << 16, 1 << 18};
        for (auto e: exp_part_size_vals) {
            CopphicContender<1, padding, min_max, bucket_t, bucketer>(N, s, e).run();
        }
    }
};


void copphicContenderRunner(size_t N) {
    dispatchPaddingBits<true, uint64_t, 6, cophobic::poly_bucketer<2>>(N);
    dispatchPaddingBits<true, uint64_t, 6, cophobic::poly_bucketer<3>>(N);
    dispatchPaddingBits<true, uint64_t, 6, cophobic::opt_bucketer>(N);

    dispatchPaddingBits<false, uint64_t, 6, cophobic::poly_bucketer<2>>(N);
    dispatchPaddingBits<false, uint64_t, 6, cophobic::poly_bucketer<3>>(N);
    dispatchPaddingBits<false, uint64_t, 6, cophobic::opt_bucketer>(N);

    dispatchPaddingBits<true, uint32_t, 6, cophobic::poly_bucketer<2>>(N);
    dispatchPaddingBits<true, uint32_t, 6, cophobic::poly_bucketer<3>>(N);
    dispatchPaddingBits<true, uint32_t, 6, cophobic::opt_bucketer>(N);

    dispatchPaddingBits<false, uint32_t, 6, cophobic::poly_bucketer<2>>(N);
    dispatchPaddingBits<false, uint32_t, 6, cophobic::poly_bucketer<3>>(N);
    dispatchPaddingBits<false, uint32_t, 6, cophobic::opt_bucketer>(N);
}
