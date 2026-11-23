#pragma once

#include "Contender.h"
#include <ShockHash2Flat.h>

template<int l>
class BipartiteShockHashFlatContender : public Contender {
    public:
        shockhash::ShockHash2Flat<l> *shockHash = nullptr;

        BipartiteShockHashFlatContender(size_t N)
                : Contender(N, 1.0) {
        }

        ~BipartiteShockHashFlatContender() override {
            delete shockHash;
        }

        std::string name() override {
            return std::string("BipartiteShockHashFlat")
                  + " l=" + std::to_string(l);
        }

        void beforeConstruction(const std::vector<std::string> &keys) override {
            (void) keys;
        }

        void construct(const std::vector<std::string> &keys) override {
            shockHash = new shockhash::ShockHash2Flat<l>(keys);
        }

        size_t sizeBits() override {
            return shockHash->getBits();
        }

        void performQueries(const std::span<std::string> keys) override {
            doPerformQueries(keys, *shockHash);
        }

        void performTest(const std::span<std::string> keys) override {
            doPerformTest(keys, *shockHash);
        }

        size_t lookup(std::string &key) override {
            return (*shockHash)(key);
        }

        bool hasLookup() const override { return true; }
};

void bipartiteShockHashFlatContenderRunner(size_t N);