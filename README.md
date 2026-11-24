# MPHF-Experiments

Comparison of a wide range different minimal perfect hash functions (MPHFs).
From these, it can generate comprehensive plots like Pareto plots, and simple comparison tables used in several papers.

<img src="img/preview-dominance-map.png" width="500"/>

The framework provides a unified interface to test basically all modern MPHF constructions that are currently available, including:

- Bucket Placement
  - CHD ([Paper](https://doi.org/10.1007/978-3-642-04128-0_61), [Code](https://cmph.sourceforge.net/))
  - PHOBIC ([Paper](https://doi.org/10.4230/LIPIcs.ESA.2024.69), [Code](https://github.com/jermp/pthash))
  - FCH ([Paper](https://doi.org/10.1145/133160.133209), [Code](https://cmph.sourceforge.net/))
  - FCH (Re-Implementation by Pibiri) ([Code](https://github.com/roberto-trani/mphf_benchmark/blob/main/include/fch.hpp))
  - PHOBIC-GPU ([Paper](https://doi.org/10.4230/LIPIcs.ESA.2024.69), [Code](https://github.com/stefanfred/PHOBIC-GPU))
  - PTHash ([Paper](https://doi.org/10.1145/3404835.3462849), [Code](https://github.com/jermp/pthash))
  - PTHash-HEM ([Paper](https://doi.org/10.1109/TKDE.2023.3303341), [Code](https://github.com/jermp/pthash))
  - PHast, PHast+ ([Paper](https://arxiv.org/pdf/2504.17918), [Code](https://github.com/beling/bsuccinct-rs/))
  - PtrHash ([Paper](https://doi.org/10.48550/ARXIV.2502.15539), [Code](https://github.com/RagnarGrootKoerkamp/PTRHash))
- Fingerprinting
  - BBHash ([Paper](https://doi.org/10.4230/LIPICS.SEA.2017.25), [Code](https://github.com/rizkg/BBHash))
  - FiPS ([Paper](https://doi.org/10.5445/IR/1000176432), [Code](https://github.com/ByteHamster/FiPS))
  - FMPH ([Paper](https://doi.org/10.1145/3596453), [Code](https://github.com/beling/bsuccinct-rs/))
  - FMPH-GO ([Paper](https://doi.org/10.1145/3596453), [Code](https://github.com/beling/bsuccinct-rs/))
- RecSplit
  - RecSplit ([Paper](https://doi.org/10.1137/1.9781611976007.14), [Code](https://github.com/vigna/sux/blob/master/sux/function/RecSplit.hpp))
  - GpuRecSplit ([Paper](https://doi.org/10.4230/LIPICS.ESA.2023.19), [Code](https://github.com/ByteHamster/GpuRecSplit))
  - SIMDRecSplit, RecSplit with rotation fitting ([Paper](https://doi.org/10.4230/LIPICS.ESA.2023.19), [Code](https://github.com/ByteHamster/GpuRecSplit))
  - Consensus-RS ([Paper](https://doi.org/10.48550/ARXIV.2502.05613), [Code](https://github.com/ByteHamster/ConsensusRecSplit/))
- Retrieval
  - BDZ / BPZ ([Paper](https://doi.org/10.1145/1321440.1321532), [Code](https://cmph.sourceforge.net/))
  - BMZ ([Paper](https://www.researchgate.net/publication/228715398_A_new_algorithm_for_constructing_minimal_perfect_hash_functions), [Code](https://cmph.sourceforge.net/))
  - CHM ([Paper](https://doi.org/10.1016/0020-0190\(92\)90220-P), [Code](https://cmph.sourceforge.net/))
  - WBPM ([Paper](https://doi.org/10.1609/AAAI.V34I02.5529), [Code](https://github.com/weaversa/MPHF-WBPM))
  - SicHash ([Paper](https://doi.org/10.1137/1.9781611977561.CH15), [Code](https://github.com/ByteHamster/SicHash))
- ShockHash
  - ShockHash (+ SIMD version) ([Paper](https://doi.org/10.1137/1.9781611977929.15), [Code](https://github.com/ByteHamster/ShockHash))
  - Bipartite ShockHash ([Paper](https://doi.org/10.48550/ARXIV.2310.14959), [Code](https://github.com/ByteHamster/ShockHash))
  - Bipartite ShockHash-Flat ([Paper](https://doi.org/10.48550/ARXIV.2310.14959), [Code](https://github.com/ByteHamster/ShockHash))
  - MorphisHash ([Paper](https://doi.org/10.48550/ARXIV.2503.10161), [Code](https://github.com/stefanfred/MorphisHash))
  - MorphisHash-Flat ([Paper](https://doi.org/10.48550/ARXIV.2503.10161), [Code](https://github.com/stefanfred/MorphisHash))

### Cloning the Repository

This repository contains submodules.
To clone the repository including submodules, use the following command.

```
git clone --recursive https://github.com/ByteHamster/MPHF-Experiments.git
```

### Running the Experiments Directly

Compiling works like with every cmake project.

```
cmake -B ./build -DCMAKE_BUILD_TYPE=Release
cmake --build ./build -j
```

This might take about 5-15 minutes because of the large number of competitors.
You can then run one of the benchmarks, for example `./build/TablePtrHash --help` or `./build/Comparison --help`.

### Code Structure

The main comparison code can be found in the [`src` directory](/src).
This includes tabular comparisons like they are used in different papers, as well as the more general Pareto plot in [`src/Comparison.cpp`](/src/Comparison.cpp).
To add a new competitor to the framework, have a look at the [`contenders` directory](/contenders).
For each contender, there are two files. (1) A general wrapper header class that unifies the interface of the competitor, and (2) a cpp file that tests a wide range of configurations for the general Pareto plot.
The cpp file should contain all meaningful configurations to cover all possible trade-offs.
After adding a contender, make sure to re-run cmake.
If you want to add a new comparison table, make sure to also adapt the [`CMakeLists.txt` file](/CMakeLists.txt) accordingly.

### Running the Experiments with Docker

For easier reproducibility and less setup overhead, we provide a docker image to run the experiments.
However, for the measurements in the papers, we run the code directly and with more data points.
We refer to [Docker.md](/Docker.md) for details on how to use this repository with Docker.

### License

This code is licensed under the [GPLv3](/LICENSE).

## k-perfect hashing and static hash sets (branch `kphf-and-static-sets`)

This branch merges [engineering-k-perfect-hashing](https://github.com/stefanfred/engineering-k-perfect-hashing)
and [static-hash-sets](https://github.com/RagnarGrootKoerkamp/static-hash-sets) into the framework.

### k > 1 and load factors
Every `Contender` has a `k_contender` (default 1) and a load factor `alpha = N / (k * M)`, where `M` is the number of bins.
The result line contains `k=` next to `loadFactor=`. `--k` and `--loadFactor` take comma separated lists.
Every method is listed in `src/registry.cpp` with a `supports(k, alpha)` predicate, e.g. RecSplit only supports `k=1, alpha=1`,
BDZ only `k=1, alpha<=0.8`, k-RecSplit/PaCHash/threshold-based bumping (without consensus) `alpha=1` and a compiled-in `k`
(see `lib/DispatchK.h`), Hash-Displace and threshold-based bumping with consensus any `alpha<=1` and a compiled-in `k`,
and CHD from cmph supports `k>=2, alpha<1`. Unsupported combinations are skipped.

#### Non-minimal threshold-based bumping and bucket placement
- `--thresholdBasedConsensus` with `alpha<1` uses a budget of `ceil(N/k)/alpha` bins. Every layer gets
  `(keys of the layer)/(k*gamma)` bins (`gamma` is the `overload=` in the name, `gamma<1` is allowed for `alpha<1`).
  Layers are added until all keys are placed or the budget is used up, and only keys left over after that are
  repaired into empty slots (FiPS + Elias-Fano, as in the minimal case). No repair structure is built otherwise.
  Repair is avoided if a layer fills its bins to more than `alpha` on average, i.e. roughly
  `E[min(Pois(gamma*k), k)] >= alpha*k`, as long as the threshold size is large enough to reach that fill.
  Since every used bin costs `thresholdSize` bits, the space does not decrease further once `alpha` is below
  the fill that the layers reach on their own.
- `--hashDisplace` with `alpha<1` hashes keys to `[0, alpha)` instead of `[0, 1)` of the optimal bucket assignment
  function and rescales the number of buckets accordingly, i.e., it only uses the `[0, alpha]` prefix of the
  bucket function for the minimal case, like suggested in the PHOBIC paper.

```bash
./Comparison --list -k 1,8 -l 0.9,1                   # what runs for which setting
./Comparison --all -k 1,4,8,16 -l 0.8,0.9,0.95,1 -n 5M # everything that supports a setting
./Comparison --kChd --hashDisplace -k 8 -l 0.9,1       # explicit selection (old flags still work)
```
Old invocations (`--pthash --minimalOnly --loadFactor 0.9`) behave as before. Variants that build a minimal function on
top of a sparser table print `loadFactor=1`, so they end up in the `alpha=1` group.

### Pareto fronts
`scripts/pareto.sh build` sweeps a (k, alpha) grid and `scripts/pareto.py results.log [--csv] [--plot dir] [--y construction]`
computes one Pareto front (bits per key vs. query or construction time) per (k, alpha) over all contenders and configurations.

### Static hash sets
`./Comparison --staticSet -k 8` builds, for every contender that exposes a per-key `lookup` (all but the GPU RecSplit),
a static hash set: the contender's (k-)PHF maps a key to a bin of `k` fingerprints that is scanned (auto-vectorized)
for the fingerprint of the key. `k * wordBits / 8 = 64` gives one cache line per bin (`k=8` with 64-bit, `k=16` with
`--setWordBits 32`); `k=4` with 64-bit fingerprints matches an AVX2 register. Any other `k` works but is not aligned.
Output lines are `SETRESULT` with `nsPerQuery` for 1%/50%/99% positive queries, in a plain loop and with the bin prefetched
a few queries ahead, plus total `bitsPerElement` (PHF + fingerprints). `scripts/pareto.py --set` computes the fronts.
Note that queries are strings, so hashing of the key is part of the measured time.

The Rust sets that do not rely on perfect hashing are in `static-hash-sets/` (hashbrown/SwissTable, eager/lazy Cuckoo,
Bloom/Cuckoo filters, fph-table, MapEmbed, TinyPtr, TPHT, a mock set measuring memory throughput):
`cd static-hash-sets && cargo run -r -F extc,ext`. The PHF-based Rust sets (k-PtrHash set, PtrHash/PHast set,
engineering-kphf wrappers) were dropped since every PHF is a contender in the C++ benchmark now.
The `tinyptr`/`tpht` third-party sources were not registered as submodules in the original repository, so they are
missing here and the `tpht` feature is off.
