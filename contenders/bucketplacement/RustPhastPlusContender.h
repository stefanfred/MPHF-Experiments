#pragma once

#include "RustContender.h"

extern "C" {
    void *createPhastPlusStruct();
    void constructPhastPlus(void *rustStruct, void *keysStruct, uint8_t bits_per_seed, uint16_t bucket_size100, size_t threads, bool ef);
    uint64_t queryPhastPlus(void *rustStruct, const char *key, size_t length);
    void queryPhastPlusAll(void *rustStruct, void *keysStruct);
    size_t sizePhastPlus(void *rustStruct);
    void destroyPhastPlusStruct(void *rustStruct);
}

class RustPhastPlusContender : public RustContender {
    protected:
        void *rustStruct = nullptr;
        uint8_t bits_per_seed;
        uint16_t bucket_size100;
        bool use_ef;
    public:
        RustPhastPlusContender(size_t N, uint8_t bits_per_seed, uint16_t bucket_size100, bool ef = false)
            : RustContender(N), bits_per_seed(bits_per_seed), bucket_size100(bucket_size100), use_ef(ef) {
            rustStruct = createPhastPlusStruct();
        }

        ~RustPhastPlusContender() override {
            destroyPhastPlusStruct(rustStruct);
        }

        std::string name() override {
            return std::string("RustPHastPlus")
                + " bits_per_seed=" + std::to_string(bits_per_seed)
                + " bucket_size100=" + std::to_string(bucket_size100)
                + " encoder=" + (use_ef ? "EF" : "C");
        }

        void construct(void *keys) override {
            constructPhastPlus(rustStruct, keys, bits_per_seed, bucket_size100, numThreads, use_ef);
        }

        size_t sizeBits() override {
            return sizePhastPlus(rustStruct) * 8;
        }

        void performQueries(void *keys) override {
            queryPhastPlusAll(rustStruct, keys);
        }

        void performTest(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return queryPhastPlus(rustStruct, key.c_str(), key.length()); };
            doPerformTest(keys, x);
        }
};

void rustPHastPlusContenderRunner(size_t N);
