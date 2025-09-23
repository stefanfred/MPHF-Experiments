#include "RustPhastContender.h"

void rustPHastContenderRunner(size_t N, bool use_ef) {
    for (size_t bucket_size_100 = 100; bucket_size_100 <= 300; bucket_size_100 += 5)
        RustPhastContender(N, 4, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 160; bucket_size_100 <= 330; bucket_size_100 += 10)
        RustPhastContender(N, 5, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 260; bucket_size_100 <= 350; bucket_size_100 += 10)
        RustPhastContender(N, 6, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 320; bucket_size_100 <= 410; bucket_size_100 += 10)
        RustPhastContender(N, 7, bucket_size_100, use_ef).run();
    RustPhastContender(N, 7, 395, use_ef).run();
    RustPhastContender(N, 7, 405, use_ef).run();

    for (size_t bucket_size_100 = 350; bucket_size_100 <= 380; bucket_size_100 += 10)
        RustPhastContender(N, 8, bucket_size_100, use_ef).run();
    for (size_t bucket_size_100 = 390; bucket_size_100 <= 475; bucket_size_100 += 5)
        RustPhastContender(N, 8, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 470; bucket_size_100 <= 530; bucket_size_100 += 20)
        RustPhastContender(N, 9, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 530; bucket_size_100 <= 610; bucket_size_100 += 10)
        RustPhastContender(N, 10, bucket_size_100, use_ef).run();

    for (size_t bucket_size_100 = 570; bucket_size_100 <= 680; bucket_size_100 += 10)
        RustPhastContender(N, 11, bucket_size_100, use_ef).run();
    RustPhastContender(N, 11, 675, use_ef).run();

    for (size_t bucket_size_100 = 650; bucket_size_100 <= 740; bucket_size_100 += 10)
        RustPhastContender(N, 12, bucket_size_100, use_ef).run();
    RustPhastContender(N, 12, 735, use_ef).run();

    for (size_t bucket_size_100 = 710; bucket_size_100 <= 800; bucket_size_100 += 10)
        RustPhastContender(N, 13, bucket_size_100, use_ef).run();
}

void rustPHastContenderRunner(size_t N) {
    rustPHastContenderRunner(N, true);
    rustPHastContenderRunner(N, false);
}
