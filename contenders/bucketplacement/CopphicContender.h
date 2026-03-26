#pragma once

#include <memory>
#include <string>
#include <vector>
#include <span>
#include <cophobic.hpp>

#undef MAX_BUCKET_SIZE

#include "Contender.h"
#include "bucketer.hpp"
#include "gxhash.hpp"

template<size_t scramble_padding_bytes = 1, size_t padding_bits = 2, bool min_max = true, typename bucket_t = uint32_t, typename bucketer_t = cophobic::poly_bucketer<3>>
class CopphicContender : public Contender {
public:
    double target_space;
    size_t exp_part_size;
    cophobic::SinglePHF<bucketer_t, scramble_padding_bytes, padding_bits, min_max, bucket_t, std::string, GxHash> *phf;

    CopphicContender(size_t N, double target_space, size_t exp_part_size)
            : Contender(N, 1.0), target_space(target_space), exp_part_size(exp_part_size), phf(nullptr) {
    }

    ~CopphicContender() override {
        if (phf != nullptr) {
            delete phf;
        }
    }

    std::string name() override {
        return std::string("CoPHOBIC")
               + " target_space=" + std::to_string(target_space)
               + " exp_part_size=" + std::to_string(exp_part_size)
               + " scramble_padding_bytes=" + std::to_string(scramble_padding_bytes)
               + " padding_bits=" + std::to_string(padding_bits)
               + " min_max=" + std::string(min_max ? "true" : "false")
               + " bucket_bits=" + std::to_string(sizeof(bucket_t) * 8)
               + " bucketer=" + bucketer_t::name();
    }

    void construct(const std::vector<std::string> &keys) override {
        phf = new cophobic::SinglePHF<bucketer_t, scramble_padding_bytes, padding_bits, min_max, bucket_t, std::string, GxHash>(
                keys, exp_part_size, target_space);
    }

    size_t sizeBits() override {
        return phf->num_bits();
    }

    void performQueries(const std::span<std::string> keys) override {
        doPerformQueries(keys, *phf);
    }

    void performTest(const std::span<std::string> keys) override {
        doPerformTest(keys, *phf);
    }
};

void copphicContenderRunner(size_t N);
