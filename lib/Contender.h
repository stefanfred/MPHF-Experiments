#pragma once

#include <random>
#include <iostream>
#include <chrono>
#include <bytehamster/util/XorShift64.h>
#include <unistd.h>
#include <thread>
#include <span>
#include "BenchmarkData.h"

#define DO_NOT_OPTIMIZE(value) asm volatile ("" : : "r,m"(value) : "memory")

class Contender {
    public:
        static size_t numQueries;
        static size_t numThreads;
        static size_t numQueryThreads;
        static size_t seed;
        static bool skipTests;

        const size_t N;
        const double loadFactor;
        const double mByN;
        const size_t M;
        long constructionTimeMicroseconds = 0;
        long queryTimeMilliseconds = 0;

        Contender(size_t N, double loadFactor)
                : N(N), loadFactor(loadFactor), mByN(1.0 / loadFactor), M(N * mByN) {
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

        virtual void performQueries(const std::span<std::string> keys) = 0;
        virtual void performTest(const std::span<std::string> keys) = 0;

        void run(bool shouldPrintResult = true) {
            if (seed == 0) {
                auto time = std::chrono::system_clock::now();
                seed = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
            }
            std::cout << std::endl;
            std::cout << "Contender: " << name().substr(0, name().find(' ')) << std::endl;
            std::vector<std::string> keys = generateInputData(N, seed);
            beforeConstruction(keys);

            std::cout << "Cooldown" << std::endl;
            usleep(1000*1000);
            std::cout << "Constructing" << std::endl;

            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            try {
                construct(keys);
            } catch (const std::exception& e) {
                std::cout<<"Error: "<<e.what()<<std::endl;
                return;
            }
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            constructionTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

            if (!skipTests) {
                std::cout<<"Testing"<<std::endl;
                performTest(keys);
            }

            queryTimeMilliseconds = 0;
            if (numQueries > 0) {
                std::cout<<"Preparing query plan"<<std::endl;
                std::vector<std::string> queryPlan;
                queryPlan.reserve(numQueries * numQueryThreads);
                bytehamster::util::XorShift64 prng(0xbf58476d1ce4e5b9 ^ seed);
                for (size_t i = 0; i < numQueries * numQueryThreads; i++) {
                    queryPlan.push_back(keys[prng(N)]);
                }
                beforeQueries(queryPlan);
                std::cout << "Cooldown" << std::endl;
                usleep(1000*1000);
                std::cout<<"Querying"<<std::endl;
                if (numQueryThreads == 1) {
                    begin = std::chrono::steady_clock::now();
                    performQueries(queryPlan);
                    end = std::chrono::steady_clock::now();
                    queryTimeMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                } else {
                    std::vector<std::thread> threads;
                    begin = std::chrono::steady_clock::now();
                    for (size_t i = 0; i < numQueryThreads; i++) {
                        std::span<std::string> querySpan(queryPlan.begin() + i * numQueries,
                                                         queryPlan.begin() + (i + 1) * numQueries);
                        threads.emplace_back([&querySpan, this] {
                            performQueries(querySpan);
                        });
                    }
                    for (size_t i = 0; i < numQueryThreads; i++) {
                        threads.at(i).join();
                    }
                    end = std::chrono::steady_clock::now();
                    queryTimeMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                }
            }
            if (shouldPrintResult) {
                printResult();
            }
        }

        void printResult(std::string additional = "") {
            // Some competitors print stuff when determining their space consumption
            double bitsPerElement = (double) sizeBits() / N;
            std::cout << "RESULT"
                      << " name=" << name()
                      << " bitsPerElement=" << bitsPerElement
                      << " constructionTimeMilliseconds=" << (constructionTimeMicroseconds < 10000
                                            ? std::to_string(0.001 * constructionTimeMicroseconds)
                                            : std::to_string(constructionTimeMicroseconds / 1000))
                      << " queryTimeMilliseconds=" << queryTimeMilliseconds
                      << " numQueries=" << numQueries
                      << " numQueriesTotal=" << (numQueries * numQueryThreads)
                      << " N=" << N
                      << " loadFactor=" << loadFactor
                      << " threads=" << numThreads
                      << " queryThreads=" << numQueryThreads
                      << additional
                      << std::endl;
        }

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
            std::vector<bool> taken(M * eps);
            for (size_t i = 0; i < keys.size(); i++) {
                // Some contenders expect non-const keys but actually use them as const.
                size_t retrieved = hashFunction(const_cast<std::string &>(keys[i]));
                if (retrieved > M * eps) {
                    std::cout << "Error: Range wrong. Hash function returned " << retrieved
                            << " but maximum should be " << (M*eps) << " (actually " << M << ")" << std::endl;
                    throw std::logic_error("Range wrong");
                }
                if (taken[retrieved]) {
                    std::cout<<"Error: Collision: Key #"<<i<<"/"<<N<<" resulted in "<<retrieved<<std::endl;
                    std::cout<<"Aborting query"<<std::endl;
                    throw std::logic_error("Collision");
                }
                taken[retrieved] = true;
            }
        }
};

size_t Contender::numQueries = 1e8;
size_t Contender::numThreads = 1;
size_t Contender::numQueryThreads = 1;
size_t Contender::seed = 0;
bool Contender::skipTests = false;
