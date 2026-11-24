//! Compare the ad-hoc seed score (2^l) with the natural score (number of ways
//! to place the remaining keys): bumped keys, construction time and query time
//! at equal space.
//!
//! `cargo run -r --example score -- [n] [k ...]`, e.g. `-- 10000000 16 32 64`.

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

struct Stats {
    bumped_pct: f32,
    build_s: f32,
    /// ns/key, plain loop and with prefetching (best of 3).
    loop_ns: f32,
    prefetch_ns: f32,
}

/// Construction can fail (assertion in the fallback structure when only a handful of keys are
/// bumped, so it gets a single bucket). Report that instead of aborting the comparison.
fn measure<const K: usize, const SCORE: u8>(keys: &[u32], alpha: f32, bits: f32) -> Option<Stats> {
    let n = keys.len();
    let start = std::time::Instant::now();
    let kphf = std::panic::catch_unwind(|| KptrHash::<MODE, K, SCORE>::new(alpha, bits, keys))
        .ok()??;
    let build_s = start.elapsed().as_secs_f32();

    let (mut loop_ns, mut prefetch_ns) = (f32::MAX, f32::MAX);
    for _ in 0..3 {
        let start = std::time::Instant::now();
        let mut c = 0;
        for &key in keys {
            c += kphf.get(key);
        }
        std::hint::black_box(c);
        loop_ns = loop_ns.min(start.elapsed().as_nanos() as f32 / n as f32);

        let start = std::time::Instant::now();
        let lookahead = 32;
        let mut c = 0;
        for i in 0..n.saturating_sub(lookahead) {
            kphf.prefetch(keys[i + lookahead]);
            c += kphf.get(keys[i]);
        }
        std::hint::black_box(c);
        prefetch_ns = prefetch_ns.min(start.elapsed().as_nanos() as f32 / n as f32);
    }
    Some(Stats {
        bumped_pct: kphf.num_bumped() as f32 / n as f32 * 100.0,
        build_s,
        loop_ns,
        prefetch_ns,
    })
}

fn compare<const K: usize>(keys: &[u32], alphas: &[f32], factors: &[f32]) {
    for &alpha in alphas {
        let lb = space_lower_bound(K, alpha);
        for &factor in factors {
            let bits = lb * factor;
            let a = measure::<K, ADHOC>(keys, alpha, bits);
            let b = measure::<K, NATURAL>(keys, alpha, bits);
            let f = |x: &Option<Stats>, g: fn(&Stats) -> f32| match x {
                Some(x) => format!("{:>6.3}", g(x)),
                None => " FAILED".to_string(),
            };
            let line = |name: &str, g: fn(&Stats) -> f32| {
                format!("{name} {} {}", f(&a, g), f(&b, g))
            };
            println!(
                "k={K:<3} a={alpha:<4} {factor:>4.2}xLB {bits:.4}b/key | {} | {} | {} | {}   (adhoc natural)",
                line("bumped%", |s| s.bumped_pct),
                line("build s", |s| s.build_s),
                line("query ns", |s| s.loop_ns),
                line("+prefetch ns", |s| s.prefetch_ns),
            );
        }
    }
}

fn main() {
    let mut args = std::env::args().skip(1);
    let n: usize = args.next().map_or(1_000_000, |s| s.parse().unwrap());
    let ks: Vec<usize> = args.map(|s| s.parse().unwrap()).collect();
    let all = ks.is_empty();
    let want = |k: usize| all || ks.contains(&k);
    let keys = gen_keys(n);
    std::panic::set_hook(Box::new(|_| {}));
    if want(8) {
        compare::<8>(&keys, &[0.5, 0.8, 0.9], &[1.2, 1.5, 2.0]);
    }
    if want(16) {
        compare::<16>(&keys, &[0.8, 0.9, 0.95], &[1.2, 1.5, 2.0]);
    }
    if want(32) {
        compare::<32>(&keys, &[0.9, 0.95], &[1.5, 2.0]);
    }
    if want(64) {
        compare::<64>(&keys, &[0.9, 0.95], &[1.5, 2.0]);
    }
    if want(128) {
        compare::<128>(&keys, &[0.9, 0.95], &[1.5, 2.0]);
    }
}
