#pragma once

#include <cmph.h>

#include <utility>
#undef MAX_BUCKET_SIZE
#include <cassert>

#include "Contender.h"

class KChdContender : public Contender {
    public:
        cmph_t *mphf = nullptr;
        cmph_io_adapter_t *source = nullptr;
        const char **data;
        int keysPerBucket;
        bool minimal;

        KChdContender(size_t N, size_t k, double loadFactor, int keysPerBucket, bool minimal)
                : Contender(N, k, minimal ? 1.0 : loadFactor), keysPerBucket(keysPerBucket), minimal(minimal) {
            data = static_cast<const char **>(malloc(N * sizeof(char*)));
            assert(1 <= keysPerBucket && keysPerBucket <= 15);
        }

        ~KChdContender() override {
            if (mphf != nullptr) {
                cmph_destroy(mphf);
            }
            free(source);
            free(data);
        }

        std::string name() override {
            return std::string("cmph-CHD")
                    + " keysPerBucket=" + std::to_string(keysPerBucket);
        }

        void beforeConstruction(const std::vector<std::string> &keys) override {
            std::cout << "Converting input" << std::endl;
            for (size_t i = 0; i < N; i++) {
                data[i] = keys[i].c_str();
            }
            source = cmph_io_vector_adapter((char **)data, N); // They even do the const cast in their readme file
        }

        void construct(const std::vector<std::string> &keys) override {
            (void) keys;
            //Create mphf
            cmph_config_t *config = cmph_config_new(source);
            cmph_config_set_algo(config, minimal ? CMPH_CHD : CMPH_CHD_PH);
            cmph_config_set_verbosity(config, 0);
            cmph_config_set_keys_per_bin(config, k_contender);
            cmph_config_set_graphsize(config, loadFactor);
            cmph_config_set_b(config, keysPerBucket);
            mphf = cmph_new(config);

            cmph_config_destroy(config);
            if (mphf == nullptr) {
                throw std::logic_error("Unable to create minimum perfect hashing function");
            }
        }

        size_t sizeBits() override {
            return 8 * cmph_packed_size(mphf);
        }

        void performQueries(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return cmph_search(mphf, key.c_str(), key.length());
            };
            doPerformQueries(keys, x);
        }

        void performTest(const std::span<std::string> keys) override {
            auto x = [&] (std::string &key) {
                return cmph_search(mphf, key.c_str(), key.length());
            };
            doPerformTest(keys, x);
        }

        size_t lookup(std::string &key) override {
            auto x = [&] (std::string &key) {
                return cmph_search(mphf, key.c_str(), key.length());
            };
            return x(key);
        }

        bool hasLookup() const override { return true; }
};

void kChdContenderRunner(size_t N, size_t k, double loadFactor);
