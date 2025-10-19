#pragma once

#include "RustContender.h"

extern "C" {
    void *createPhastPlusWrappedStruct();
    void constructPhastPlusWrapped(void *rustStruct, void *keysStruct, uint8_t multiplier, uint8_t bits_per_seed, uint16_t bucket_size100, uint16_t preferred_slice_len, size_t threads, bool ef);
    uint64_t queryPhastPlusWrapped(void *rustStruct, const char *key, size_t length);
    void queryPhastPlusWrappedAll(void *rustStruct, void *keysStruct);
    size_t sizePhastPlusWrapped(void *rustStruct);
    void destroyPhastPlusWrappedStruct(void *rustStruct);
}

class RustPhastPlusWrappedContender : public RustContender {
    protected:
        void *rustStruct = nullptr;
        uint16_t multiplier;
        uint8_t bits_per_seed;
        uint16_t bucket_size100;
        uint16_t preferred_slice_len;
        bool use_ef;
    public:
        RustPhastPlusWrappedContender(size_t N, uint8_t multiplier, uint8_t bits_per_seed,
                        uint16_t bucket_size100, uint16_t preferred_slice_len, bool ef)
            : RustContender(N), multiplier(multiplier), bits_per_seed(bits_per_seed),
              bucket_size100(bucket_size100), preferred_slice_len(preferred_slice_len), use_ef(ef) {
            rustStruct = createPhastPlusWrappedStruct();
        }

        ~RustPhastPlusWrappedContender() override {
            destroyPhastPlusWrappedStruct(rustStruct);
        }

        std::string name() override {
            return std::string("RustPHastPlusWrapped")
                + " bits_per_seed=" + std::to_string(bits_per_seed)
                + " bucket_size100=" + std::to_string(bucket_size100)
                + " multiplier=" + std::to_string(multiplier)
                + " preferred_slice_len=" + std::to_string(preferred_slice_len)
                + " encoder=" + (use_ef ? "EF" : "C");
        }

        void construct(void *keys) override {
            constructPhastPlusWrapped(rustStruct, keys, multiplier, bits_per_seed,
                                    bucket_size100, preferred_slice_len, numThreads, use_ef);
        }

        size_t sizeBits() override {
            return sizePhastPlusWrapped(rustStruct) * 8;
        }

        void performQueries(void *keys) override {
            queryPhastPlusWrappedAll(rustStruct, keys);
        }

        void performTest(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return queryPhastPlusWrapped(rustStruct, key.c_str(), key.length()); };
            doPerformTest(keys, x);
        }
};

void rustPHastPlusWrappedContenderRunner(size_t N);
