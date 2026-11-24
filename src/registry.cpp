#include "Registry.h"
#include "Contender.h"
#include "DispatchK.h"

#include "fingerprinting/BBHashContender.h"
#include "retrievalbased/SicHashContender.h"
#include "bucketplacement/PTHashContender.h"
#include "recsplit/RecSplitContender.h"
#include "recsplit/RecSplitRotateContender.h"
#include "retrievalbased/MphfWbpmContender.h"
#include "shockhash/ShockHashContender.h"
#include "bucketplacement/PartitionedPTHashContender.h"
#include "fingerprinting/RustFmphContender.h"
#include "fingerprinting/RustFmphGoContender.h"
#include "bucketplacement/RustPhastContender.h"
#include "bucketplacement/RustPtrHashContender.h"
#include "shockhash/BipartiteShockHashContender.h"
#include "bucketplacement/FchContender.h"
#include "bucketplacement/PhobicContender.h"
#include "recsplit/ConsensusContender.h"
#include "shockhash/BipartiteShockHashFlatContender.h"
#include "fingerprinting/FiPSContender.h"
#include "bucketplacement/ChdContender.h"
#include "retrievalbased/BdzContender.h"
#include "retrievalbased/BmzContender.h"
#include "retrievalbased/ChmContender.h"
#include "bucketplacement/FchCmphContender.h"
#include "shockhash/MorphisHashContender.h"
#include "shockhash/MorphisHashFlatContender.h"
#include "bucketplacement/RustPhastPlusWrappedContender.h"
#include "kphf/KChdContender.hpp"
#include "kphf/HashDisplaceContender.hpp"
#include "kphf/PaCHashContender.hpp"
#include "kphf/ThresholdBasedBumpingContender.hpp"
#include "kphf/ThresholdBasedBumpingConsensusContender.hpp"
#include "kphf/ThresholdBasedBumpingOldContender.hpp"
#include "kphf/KRecSplitContender.hpp"

#ifdef HAS_VULKAN
#include "bucketplacement/GpuPhobicContender.h"
#endif

#ifdef SIMD
#include "recsplit/SIMDRecSplitContender.h"
#include "shockhash/ShockHashSimdContender.h"
#endif

using namespace support;

// Predicates ------------------------------------------------------------------------------------------------------
// Plain perfect hash functions without a tunable load factor: only k=1, alpha=1.
static bool minimalPhfOnly(size_t k, double alpha) { return k == 1 && isOne(alpha); }
// Methods that place keys into a range of size N/alpha, e.g. via bucket placement or hypergraph peeling.
static bool phfAnyLoad(size_t k, double alpha) { return k == 1 && alpha > 0 && alpha <= 1.0 + 1e-9; }
// cmph's BDZ (non-minimal) only works for sparse load
static bool bdzSupports(size_t k, double alpha) { return k == 1 && alpha > 0 && alpha <= 0.8 + 1e-9; }
// k-perfect hash functions with compile-time k that are built for (k-)minimal output
static bool kMinimal(size_t k, double alpha) { return isCompiledK(k) && isOne(alpha); }
// k-perfect hash functions with compile-time k that support any load factor alpha <= 1
static bool kAnyLoad(size_t k, double alpha) { return isCompiledK(k) && alpha > 0 && alpha <= 1.0 + 1e-9; }
// CHD from cmph can be configured for arbitrary k, but never terminates for alpha=1
static bool kChdSupports(size_t k, double alpha) { return k >= 2 && alpha > 0 && alpha < 0.995; }

// Runners ---------------------------------------------------------------------------------------------------------
#define RUN_N(fn) [](const RunConfig &c) { fn(c.N); }
#define RUN_NA(fn) [](const RunConfig &c) { fn(c.N, c.alpha); }
#define RUN_NK(fn) [](const RunConfig &c) { fn(c.N, c.k); }

// ShockHash has a different runner if SIMD is available
static void shockHashRun(const RunConfig &c) {
    #ifdef SIMD
        shockHashSimdContenderRunner(c.N);
    #else
        shockHashContenderRunner(c.N);
    #endif
}

const std::vector<ContenderSpec> &allContenders() {
    static const std::vector<ContenderSpec> list = {
        // ---- k = 1 ----
        {"recsplit", "RecSplit", minimalPhfOnly, RUN_N(recSplitContenderRunner)},
        {"recSplitRotate", "RecSplit with rotation fitting", minimalPhfOnly, RUN_N(recSplitRotateContenderRunner)},
        {"simdrecsplit", "SIMDRecSplit",
         [](size_t k, double a) {
            #ifdef SIMD
                return minimalPhfOnly(k, a);
            #else
                (void) k; (void) a; return false;
            #endif
         },
         [](const RunConfig &c) {
            #ifdef SIMD
                simdRecSplitContenderRunner(c.N);
            #else
                (void) c;
            #endif
         }},
        {"mphfWbpm", "MPHF-WBPM", minimalPhfOnly, RUN_N(mphfWbpmContenderRunner)},
        {"bbhash", "BBHash", minimalPhfOnly, RUN_N(bbHashContenderRunner)},
        {"sichash", "SicHash", phfAnyLoad,
         [](const RunConfig &c) { sicHashContenderRunner(c.N, c.alpha, c.minimalOnly, c.sichashOnlyPartial); }},
        {"shockhash", "ShockHash", minimalPhfOnly, shockHashRun},
        {"bipartiteShockHash", "Bipartite ShockHash", minimalPhfOnly, RUN_N(bipartiteShockHashContenderRunner)},
        {"bipartiteShockHashFlat", "Bipartite ShockHash-Flat", minimalPhfOnly, RUN_N(bipartiteShockHashFlatContenderRunner)},
        {"morphisHash", "MorphisHash", minimalPhfOnly, RUN_N(morphisHashContenderRunner)},
        {"morphisHashFlat", "MorphisHash-Flat", minimalPhfOnly, RUN_N(morphisHashFlatContenderRunner)},
        {"pthash", "PTHash", phfAnyLoad,
         [](const RunConfig &c) { ptHashContenderRunner(c.N, c.alpha, c.minimalOnly); }},
        {"partitionedPthash", "Partitioned PTHash", phfAnyLoad,
         [](const RunConfig &c) { partitionedPtHashContenderRunner(c.N, c.alpha, c.minimalOnly); }},
        {"densePartitionedPtHash", "PHOBIC (dense partitioned PTHash)", minimalPhfOnly, RUN_N(PhobicContenderRunner)},
        {"gpuPhobic", "PHOBIC on the GPU",
         [](size_t k, double a) {
            #ifdef HAS_VULKAN
                return minimalPhfOnly(k, a);
            #else
                (void) k; (void) a; return false;
            #endif
         },
         [](const RunConfig &c) {
            #ifdef HAS_VULKAN
                gpuPhobicContenderRunner(c.N);
            #else
                (void) c;
            #endif
         }},
        {"chd", "CHD (cmph), k=1", phfAnyLoad, RUN_NA(chdContenderRunner)},
        {"bdz", "BDZ (cmph)", bdzSupports, RUN_NA(bdzContenderRunner)},
        {"bmz", "BMZ (cmph)", phfAnyLoad, RUN_NA(bmzContenderRunner)},
        {"chm", "CHM (cmph)", phfAnyLoad, RUN_NA(chmContenderRunner)},
        {"fchCmph", "FCH (cmph)", phfAnyLoad, RUN_NA(fchCmphContenderRunner)},
        {"fchPtHash", "FCH (PTHash reimplementation)", minimalPhfOnly, RUN_N(fchPtHashContenderRunner)},
        {"rustFmph", "Rust FMPH", minimalPhfOnly, RUN_N(rustFmphContenderRunner)},
        {"rustFmphGo", "Rust FMPH-GO", minimalPhfOnly, RUN_N(rustFmphGoContenderRunner)},
        {"rustPHast", "Rust PHast", minimalPhfOnly, RUN_N(rustPHastContenderRunner)},
        {"rustPHastPlus", "Rust PHast+", minimalPhfOnly, RUN_N(rustPHastPlusWrappedContenderRunner)},
        {"rustPtrHash", "Rust PtrHash", minimalPhfOnly, RUN_N(rustPtrHashContenderRunner)},
        {"fiPS", "FiPS", minimalPhfOnly, RUN_N(fiPSContenderRunner)},
        {"consensus", "Consensus-RecSplit", minimalPhfOnly, RUN_N(consensusContenderRunner)},
        // ---- k >= 2 ----
        {"kChd", "CHD (cmph) with k keys per bin", kChdSupports,
         [](const RunConfig &c) { kChdContenderRunner(c.N, c.k, c.alpha); }},
        {"hashDisplace", "Hash-and-displace (optimal bucket function, [0,alpha] prefix for alpha<1)", kAnyLoad,
         [](const RunConfig &c) { hashDisplaceContenderRunner(c.N, c.k, c.alpha); }},
        {"pachash", "PaCHash", [](size_t k, double a) { return k >= 4 && isOne(a); }, RUN_NK(paCHashContenderRunner)},
        {"thresholdBased", "Threshold-based bumping", kMinimal, RUN_NK(thresholdBasedBumpingContenderRunner)},
        {"thresholdBasedConsensus", "Threshold-based bumping with consensus (any alpha, repair only if needed)", kAnyLoad,
         [](const RunConfig &c) { thresholdBasedBumpingConsensusContenderRunner(c.N, c.k, c.alpha); }},
        {"thresholdBasedOld", "Threshold-based bumping (implementation from the literature)", kMinimal,
         RUN_NK(thresholdBasedBumpingOldContenderRunner)},
        {"kRecSplit", "k-RecSplit", kMinimal, RUN_NK(kRecSplitContenderRunner)},
    };
    return list;
}
