#include "PTHashContender.h"

void ptHashContenderRunner(size_t N, double loadFactor, bool minimalOnly) {
    using dictionary_elias_fano = pthash::dual<pthash::dictionary, pthash::elias_fano>;
    for (double lambda = 3.0; lambda < 10.0; lambda += 0.4) {
        if (!minimalOnly) {
            PTHashContender<false, pthash::elias_fano>(N, loadFactor, lambda).run();
            PTHashContender<false, pthash::dictionary_dictionary>(N, loadFactor, lambda).run();
            PTHashContender<false, dictionary_elias_fano>(N, loadFactor, lambda).run();
            PTHashContender<false, pthash::compact_compact>(N, loadFactor, lambda).run();
        }

        PTHashContender<true, pthash::elias_fano>(N, loadFactor, lambda).run();
        PTHashContender<true, pthash::dictionary_dictionary>(N, loadFactor, lambda).run();
        PTHashContender<true, dictionary_elias_fano>(N, loadFactor, lambda).run();
        PTHashContender<true, pthash::compact_compact>(N, loadFactor, lambda).run();
    }
}
