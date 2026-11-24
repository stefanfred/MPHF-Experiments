//! Compare the ad-hoc seed score (2^l) with the natural score (number of ways
//! to place the remaining keys) by the fraction of bumped keys, at equal space.
//!
//! `cargo run -r --example score`

use kphf::{space_lower_bound, KptrHash, Mode, Score};

const MODE: u8 = Mode::SortBump as u8;
const ADHOC: u8 = Score::AdHoc as u8;
const NATURAL: u8 = Score::Natural as u8;

fn gen_keys(n: usize) -> Vec<u32> {
    let mut keys = std::collections::HashSet::with_capacity(n);
    let mut buf = vec![0u32; 1024];
    while keys.len() < n {
        rand::fill(&mut buf[..]);
        keys.extend(buf.iter().copied());
    }
    keys.into_iter().take(n).collect()
}

fn bumped<const K: usize, const SCORE: u8>(keys: &[u32], alpha: f32, bits: f32) -> (f32, f32) {
    let start = std::time::Instant::now();
    let kphf = KptrHash::<MODE, K, SCORE>::new(alpha, bits, keys).unwrap();
    let secs = start.elapsed().as_secs_f32();
    (kphf.num_bumped() as f32 / keys.len() as f32 * 100.0, secs)
}

fn compare<const K: usize>(keys: &[u32], alphas: &[f32], factors: &[f32]) {
    for &alpha in alphas {
        let lb = space_lower_bound(K, alpha);
        for &factor in factors {
            let bits = lb * factor;
            let (a, ta) = bumped::<K, ADHOC>(keys, alpha, bits);
            let (b, tb) = bumped::<K, NATURAL>(keys, alpha, bits);
            println!(
                "k={K:<3} alpha={alpha:<5} {factor:>4.2}x LB ({bits:.4} b/key)  \
                 bumped adhoc {a:>7.3}% ({ta:>5.2}s)  natural {b:>7.3}% ({tb:>5.2}s)"
            );
        }
    }
}

fn main() {
    let n = std::env::args()
        .nth(1)
        .map_or(1_000_000, |s| s.parse().unwrap());
    let keys = gen_keys(n);
    compare::<8>(&keys, &[0.5, 0.8, 0.9], &[1.2, 1.5, 2.0]);
    compare::<16>(&keys, &[0.8, 0.9, 0.95], &[1.2, 1.5, 2.0]);
    compare::<32>(&keys, &[0.9, 0.95], &[1.5, 2.0]);
}
