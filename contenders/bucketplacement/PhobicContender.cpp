#include "PhobicContender.h"
#include <utils/dense_encoders.hpp>

void PhobicContenderRunner(size_t N) {
    for (double lambda = 2.0; lambda < 10.0; lambda += 0.5) {
        { PhobicContender<pthash::C_mono, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
        { PhobicContender<pthash::R_mono, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
        { PhobicContender<pthash::D_mono, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
        { PhobicContender<pthash::C_int, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
        { PhobicContender<pthash::D_int, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
        { PhobicContender<pthash::R_int, pthash::opt_bucketer>(N, 1.0, lambda).run(); }
    }
}
