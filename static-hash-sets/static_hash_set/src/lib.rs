//! Static hash sets that do not rely on perfect hashing.
//! The perfect-hash-based sets live in the C++ benchmark (`Comparison --staticSet`), where every
//! (k-)PHF contender can be plugged in.

mod bin;
mod traits;

#[cfg(feature = "ext")]
pub mod cuckoo;
#[cfg(feature = "ext")]
pub mod u64_hashset;

use bin::Bin;
pub use traits::HashSet;

type T = u64;
const BIN_SIZE: usize = 8;
type S = wide::i64x4;
