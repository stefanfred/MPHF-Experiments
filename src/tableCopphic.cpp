#include <tlx/cmdline_parser.hpp>

#include "bucketplacement/CopphicContender.h"
#include "bucketplacement/RustPhastContender.h"


int main(int argc, char **argv) {
    size_t N = 5e6;
    tlx::CmdlineParser cmd;
    cmd.add_bytes('n', "numKeys", N, "Number of objects");
    cmd.add_bytes('q', "numQueries", Contender::numQueries, "Number of queries to perform");
    cmd.add_bytes('t', "numThreads", Contender::numThreads, "Number of threads to use for construction");
    cmd.add_flag('T', "skipTests", Contender::skipTests, "Skip testing PHF for validity");

    if (!cmd.process(argc, argv)) {
        return 1;
    }
    { CopphicContender<1, 4, true, uint32_t, cophobic::poly_bucketer<2>>(N, 1.9, 1 << 17).run(); }
    { RustPhastContender(N, 8, 300).run(); }

    return 0;
}
