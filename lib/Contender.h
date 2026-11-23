#pragma once

#include <random>
#include <iostream>
#include <chrono>
#include <bytehamster/util/XorShift64.h>
#include <unistd.h>
#include <thread>
#include <span>

#define DO_NOT_OPTIMIZE(value) asm volatile ("" : : "r,m"(value) : "memory")

class Contender {
    public:
        static size_t numQueries;
        static size_t numThreads;
        static size_t numQueryThreads;
        static size_t seed;
        static bool skipTests;
        /** Additionally benchmark a static hash set (bins of k fingerprints) built on top of this contender. */
        static bool staticSet;
        static size_t numSetQueries;
        /** Fingerprint width in bits (32 or 64) stored in the static set. k * wordBits/8 should be 64 (cache line). */
        static size_t setWordBits;

        const size_t N;
        /** Number of keys that may be mapped to the same output position (k-perfect hashing, k=1 is classic PHF). */
        const size_t k_contender;
        /** Load factor alpha = N / (k * M), where M is the output range (number of bins of size k). */
        const double loadFactor;
        const double mByN;
        const size_t M;
        long constructionTimeMicroseconds = 0;
        long queryTimeMilliseconds = 0;
        long queryCacheMisses = 0;
        long constructionCacheMisses = 0;

        Contender(size_t N, size_t k, double loadFactor)
                : N(N), k_contender(k), loadFactor(loadFactor), mByN(1.0 / loadFactor), M((N + k - 1) / k * mByN) {
        }

        /** Classic (k=1) perfect hash function. */
        Contender(size_t N, double loadFactor) : Contender(N, 1, loadFactor) {
        }

        virtual ~Contender() = default;

        virtual std::string name() = 0;
        virtual size_t sizeBits() = 0;
        virtual void construct(const std::vector<std::string> &keys) = 0;

        virtual void beforeConstruction(const std::vector<std::string> &keys) {
            (void) keys;
        }

        virtual void beforeQueries(const std::span<std::string> &keys) {
            (void) keys;
        }

        /** Returns the output (bin) of the (k-)perfect hash function for a single key. Only valid after construct. */
        virtual size_t lookup(std::string &key) {
            (void) key;
            throw std::logic_error("This contender does not provide a per-key lookup");
        }
        virtual bool hasLookup() const { return false; }

        virtual void performQueries(std::span<std::string> keys) = 0;
        virtual void performTest(std::span<std::string> keys) = 0;

        void run(bool shouldPrintResult = true); // In cpp file

        void printResult(const std::string &additional = ""); // In cpp file

        void runStaticSetBenchmark(const std::vector<std::string> &keys,
                                   bytehamster::util::XorShift64 &prng); // In StaticSetBenchmark.cpp

    protected:
        template<typename F>
        void doPerformQueries(const std::span<std::string> keys, F &hashFunction) {
            for (const std::string &key : keys) {
                size_t retrieved = hashFunction(const_cast<std::string &>(key));
                // Some contenders expect non-const keys but actually use them as const.
                DO_NOT_OPTIMIZE(retrieved);
            }
        }

        template<typename F>
        void doPerformTest(const std::span<std::string> keys, F &hashFunction) {
            double eps = 1.0001; // Rounding with load factor variables
            std::vector<uint32_t> taken(M * eps + 1);
            for (size_t i = 0; i < keys.size(); i++) {
                // Some contenders expect non-const keys but actually use them as const.
                size_t retrieved = hashFunction(const_cast<std::string &>(keys[i]));
                if (retrieved > M * eps) {
                    std::cout << "Error: Range wrong. Hash function returned " << retrieved
                            << " but maximum should be " << (M*eps) << " (actually " << M << ")" << std::endl;
                    throw std::logic_error("Range wrong");
                }
                if (taken[retrieved] >= k_contender) {
                    std::cout<<"Error: More than k="<<k_contender<<" keys collide: Key #"<<i<<"/"<<N
                            <<" resulted in "<<retrieved<<std::endl;
                    std::cout<<"Aborting query"<<std::endl;
                    throw std::logic_error("Collision");
                }
                taken[retrieved]++;
            }
        }
};
