#include "RustPhastPlusContender.h"

void rustPHastPlusContenderRunner(size_t N, bool use_ef) {
    for (size_t bucket_size_100 = 370; bucket_size_100 <= 540; bucket_size_100 += 5)
        RustPhastPlusContender(N, 8, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 430; bucket_size_100 <= 570; bucket_size_100 += 5)
        RustPhastPlusContender(N, 9, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 440; bucket_size_100 <= 620; bucket_size_100 += 5)
        RustPhastPlusContender(N, 10, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 480; bucket_size_100 <= 680; bucket_size_100 += 5)
        RustPhastPlusContender(N, 11, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 470; bucket_size_100 <= 715; bucket_size_100 += 5)
        RustPhastPlusContender(N, 12, bucket_size_100, use_ef).run();
}

void rustPHastPlusContenderRunner(size_t N) {
    rustPHastPlusContenderRunner(N, true);
    rustPHastPlusContenderRunner(N, false);
}
