use super::*;
#[cfg(feature = "extc")]
use fph_table::FphDynSet;
#[cfg(feature = "extc")]
use mapembed::MapEmbed;

#[test]
fn test() {
    let mut hashers = vec![
        Box::new(U64HashSet::new(1.1, &[])) as Box<dyn HashSet>,
        Box::new(CuckooSet::<{ Mode::Lazy as u8 }>::new(1.1, &[])),
        Box::new(CuckooSet::<{ Mode::Eager as u8 }>::new(1.1, &[])),
    ];

    #[cfg(feature = "ext")]
    {
        hashers.extend(vec![
            Box::new(hashbrown::HashSet::<T, gxhash::GxBuildHasher>::default()) as Box<dyn HashSet>,
        ]);
    }

    #[cfg(feature = "extc")]
    {
        hashers.extend(vec![
            Box::new(FphDynSet::new(0.9, &[]).unwrap()) as Box<dyn HashSet>,
            Box::new(MapEmbed::new(&[]).unwrap()) as Box<dyn HashSet>,
        ]);
    }

    for n in [100_000, 1_000_000] {
        let keys = (0..n as u64).collect::<Vec<_>>();
        for hasher in &hashers {
            eprintln!("Test {}", hasher.name());
            test_one(&keys, &**hasher);
        }
    }
}

fn test_one(keys: &[u64], h: &dyn HashSet) {
    let n = keys.len();
    let h = h.new(&keys);

    for i in 0..n {
        assert!(h.contains(i as u64), "Failed for {i}");
    }
    for i in n..2 * n {
        assert!(!h.contains(i as u64), "Failed for {i}");
    }
}
