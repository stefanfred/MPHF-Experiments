#include <tlx/cmdline_parser.hpp>
#include <iostream>
#include <sstream>
#include <map>
#include <set>

#include "Contender.h"
#include "Registry.h"

template <typename T>
static std::vector<T> parseList(const std::string &s) {
    std::vector<T> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        if constexpr (std::is_floating_point_v<T>) {
            result.push_back(std::stod(item));
        } else {
            result.push_back(std::stoull(item));
        }
    }
    return result;
}

int main(int argc, char** argv) {
    RunConfig config;
    std::string alphaList = "0.8";
    std::string kList = "1";
    bool all = false;
    bool list = false;
    std::map<std::string, bool> selected;

    tlx::CmdlineParser cmd;
    cmd.add_string('l', "loadFactor", alphaList,
                   "Load factor alpha = N/(k*M); comma separated list, e.g. 0.8,0.9,1 (default 0.8)");
    cmd.add_string('k', "k", kList,
                   "Keys per output position; comma separated list, e.g. 1,4,8 (default 1)");
    cmd.add_bytes('n', "numKeys", config.N, "Number of objects");
    cmd.add_bytes('q', "numQueries", Contender::numQueries, "Number of queries to perform");
    cmd.add_bytes('t', "numThreads", Contender::numThreads, "Number of threads to run benchmarks with");
    cmd.add_bytes('Q', "numQueryThreads", Contender::numQueryThreads, "Number of threads to run queries with");
    cmd.add_flag('M', "minimalOnly", config.minimalOnly,
                 "Skip generation of non-minimal variants in case the method supports both");
    cmd.add_flag('T', "skipTests", Contender::skipTests, "Skip testing PHF for validity");
    cmd.add_bytes('s', "seed", Contender::seed, "Seed for pseudo-number generator");
    cmd.add_flag("sichashOnlyPartial", config.sichashOnlyPartial,
                 "Ignore fast ribbon retrieval configurations and test fewer thresholds");
    cmd.add_flag("staticSet", Contender::staticSet,
                 "Additionally benchmark a static hash set (bins of k fingerprints) on top of each contender "
                 "that exposes a per-key lookup. Use k=8 (64-bit fingerprints) or k=16 (32-bit) for cache-line bins.");
    cmd.add_bytes("setQueries", Contender::numSetQueries, "Number of queries per static set measurement");
    cmd.add_bytes("setWordBits", Contender::setWordBits, "Fingerprint width of the static set: 32 or 64");
    cmd.add_flag('a', "all", all, "Run every contender that supports the requested (k, loadFactor) settings");
    cmd.add_flag("list", list, "List all contenders and which of the requested (k, loadFactor) settings they support");

    const auto &contenders = allContenders();
    for (const ContenderSpec &spec : contenders) {
        selected[spec.flag] = false;
        cmd.add_flag(spec.flag, selected[spec.flag], spec.description);
    }

    if (!cmd.process(argc, argv)) {
        return 1;
    }
    std::vector<size_t> ks = parseList<size_t>(kList);
    std::vector<double> alphas = parseList<double>(alphaList);
    if (ks.empty() || alphas.empty()) {
        std::cerr << "Need at least one value for k and for loadFactor" << std::endl;
        return 1;
    }

    if (list) {
        for (const ContenderSpec &spec : contenders) {
            std::cout << spec.flag << "\t";
            for (size_t k : ks) {
                for (double alpha : alphas) {
                    if (spec.supports(k, alpha)) {
                        std::cout << " (k=" << k << ",alpha=" << alpha << ")";
                    }
                }
            }
            std::cout << std::endl;
        }
        return 0;
    }

    for (size_t k : ks) {
        for (double alpha : alphas) {
            config.k = k;
            config.alpha = alpha;
            for (const ContenderSpec &spec : contenders) {
                if (!all && !selected[spec.flag]) {
                    continue;
                }
                if (!spec.supports(k, alpha)) {
                    if (!all) {
                        std::cerr << "Skipping " << spec.flag << ": does not support k=" << k
                                  << ", loadFactor=" << alpha << std::endl;
                    }
                    continue;
                }
                spec.run(config);
            }
        }
    }
    return 0;
}
