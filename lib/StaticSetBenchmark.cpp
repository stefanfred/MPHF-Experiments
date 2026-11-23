#include "Contender.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <bytehamster/util/MurmurHash64.h>
#include <bytehamster/util/XorShift64.h>

std::vector<std::string> generateInputData(size_t N, uint64_t seed); // In Contender.cpp

namespace {
/**
 * Static hash set on top of an arbitrary (k-)perfect hash function:
 * The PHF maps every key to one of M bins that hold up to k fingerprints each.
 * A query evaluates the PHF and scans the (SIMD-friendly, contiguous) bin for the fingerprint of the key.
 * With k * sizeof(W) = 64 (or the SIMD register width), a bin is one cache line (or one vector).
 */
template <typename W>
class PhfBackedSet {
    public:
        PhfBackedSet(Contender &contender, const std::vector<std::string> &keys)
                : contender(contender), k(contender.k_contender), numBins(contender.M + 2) {
            size_t bytes = numBins * k * sizeof(W);
            bytes = (bytes + 63) / 64 * 64;
            slots = static_cast<W *>(std::aligned_alloc(64, bytes));
            std::memset(slots, 0, bytes);
            std::vector<uint32_t> fill(numBins, 0);
            for (const std::string &key : keys) {
                size_t bin = contender.lookup(const_cast<std::string &>(key));
                if (bin >= numBins || fill[bin] >= k) {
                    throw std::logic_error("PHF violates its contract while inserting into the set");
                }
                slots[bin * k + fill[bin]++] = fingerprint(key);
            }
        }

        ~PhfBackedSet() { std::free(slots); }

        static W fingerprint(const std::string &key) {
            W fp = static_cast<W>(bytehamster::util::MurmurHash64(key.data(), static_cast<int>(key.length())));
            return fp == 0 ? 1 : fp; // 0 marks an empty slot
        }

        size_t slotBits() const { return numBins * k * sizeof(W) * 8; }

        inline bool probe(size_t bin, W fp) const {
            if (bin >= numBins) { // Non-members can be mapped to arbitrary values
                return false;
            }
            const W *b = slots + bin * k;
            bool found = false;
            for (size_t i = 0; i < k; i++) {
                found |= (b[i] == fp);
            }
            return found;
        }

        size_t countLoop(std::span<std::string> queries) {
            size_t c = 0;
            for (std::string &q : queries) {
                c += probe(contender.lookup(q), fingerprint(q));
            }
            return c;
        }

        /** Evaluates the PHF a few queries ahead and prefetches the bin so that the memory latency is hidden. */
        size_t countPrefetch(std::span<std::string> queries) {
            constexpr size_t L = 16;
            size_t ring[L];
            size_t c = 0;
            auto stage = [&](size_t i) {
                size_t bin = contender.lookup(queries[i]);
                if (bin < numBins) {
                    __builtin_prefetch(slots + bin * k);
                }
                ring[i % L] = bin;
            };
            size_t n = queries.size();
            for (size_t i = 0; i < std::min(L, n); i++) stage(i);
            for (size_t i = 0; i < n; i++) {
                size_t bin = ring[i % L];
                W fp = fingerprint(queries[i]);
                if (i + L < n) stage(i + L);
                c += probe(bin, fp);
            }
            return c;
        }

    private:
        Contender &contender;
        size_t k;
        size_t numBins;
        W *slots = nullptr;
};

template <typename W>
void runForWidth(Contender &contender, const std::vector<std::string> &keys, bytehamster::util::XorShift64 &prng) {
    using clock = std::chrono::steady_clock;
    std::cout << "Building static set" << std::endl;
    auto begin = clock::now();
    PhfBackedSet<W> set(contender, keys);
    double buildNsPerKey = std::chrono::duration<double, std::nano>(clock::now() - begin).count() / keys.size();

    size_t numQueries = Contender::numSetQueries;
    std::vector<std::string> negatives = generateInputData(numQueries, bytehamster::util::MurmurHash64(prng() ^ 0x9e3779b97f4a7c15ull));
    // Plain prng() seeds would yield a shifted copy of the key stream (XorShift has a single cycle), i.e. members.
    double phfBits = static_cast<double>(contender.sizeBits()) / contender.N;
    double slotBits = static_cast<double>(set.slotBits()) / contender.N;

    for (double positiveFraction : {0.01, 0.5, 0.99}) {
        std::vector<std::string> queries;
        queries.reserve(numQueries);
        for (size_t i = 0; i < numQueries; i++) {
            if (prng() < positiveFraction * static_cast<double>(UINT64_MAX)) {
                queries.push_back(keys[prng(keys.size())]);
            } else {
                queries.push_back(negatives[i]);
            }
        }
        for (const char *mode : {"loop", "prefetch"}) {
            size_t found = 0;
            begin = clock::now();
            if (mode[0] == 'l') {
                found = set.countLoop(queries);
            } else {
                found = set.countPrefetch(queries);
            }
            double ns = std::chrono::duration<double, std::nano>(clock::now() - begin).count() / numQueries;
            DO_NOT_OPTIMIZE(found);
            std::cout << "SETRESULT"
                      << " name=" << contender.name()
                      << " k=" << contender.k_contender
                      << " loadFactor=" << contender.loadFactor
                      << " N=" << contender.N
                      << " wordBits=" << (8 * sizeof(W))
                      << " binBytes=" << (contender.k_contender * sizeof(W))
                      << " mode=" << mode
                      << " positiveFraction=" << positiveFraction
                      << " nsPerQuery=" << ns
                      << " foundFraction=" << static_cast<double>(found) / numQueries
                      << " bitsPerElement=" << (phfBits + slotBits)
                      << " phfBitsPerElement=" << phfBits
                      << " slotBitsPerElement=" << slotBits
                      << " buildNsPerKey=" << buildNsPerKey
                      << " constructionTimeMilliseconds=" << contender.constructionTimeMicroseconds / 1000
                      << std::endl;
        }
    }
}
} // namespace

void Contender::runStaticSetBenchmark(const std::vector<std::string> &keys, bytehamster::util::XorShift64 &prng) {
    if (!hasLookup()) {
        std::cout << "Skipping static set benchmark: " << name() << " does not expose a per-key lookup" << std::endl;
        return;
    }
    if (setWordBits == 32) {
        runForWidth<uint32_t>(*this, keys, prng);
    } else {
        runForWidth<uint64_t>(*this, keys, prng);
    }
}
