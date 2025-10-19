use ph::GetSize;
use ph::phast::{self, DefaultCompressedArray, Params, ShiftOnlyWrapped};
use ph::phast::compressed_array::CompactFast;
use std::hint::black_box;
use std::slice;
use std::os::raw::c_char;

pub enum PHastPlusWrappedVariant {
    BitsEF(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<1>, DefaultCompressedArray>),
    Bits8EF(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<1>, DefaultCompressedArray>),
    BitsC(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<1>, CompactFast>),
    Bits8C(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<1>, CompactFast>),

    BitsEF2(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<2>, DefaultCompressedArray>),
    Bits8EF2(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<2>, DefaultCompressedArray>),
    BitsC2(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<2>, CompactFast>),
    Bits8C2(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<2>, CompactFast>),

    BitsEF3(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<3>, DefaultCompressedArray>),
    Bits8EF3(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<3>, DefaultCompressedArray>),
    BitsC3(phast::Function2<ph::seeds::BitsFast, ShiftOnlyWrapped<3>, CompactFast>),
    Bits8C3(phast::Function2<ph::seeds::Bits8, ShiftOnlyWrapped<3>, CompactFast>),
    None
}

#[no_mangle]
pub extern "C" fn createPhastPlusWrappedStruct() -> *mut PHastPlusWrappedVariant {
    Box::into_raw(Box::new(PHastPlusWrappedVariant::None))
}

#[no_mangle]
pub extern "C" fn constructPhastPlusWrapped(struct_ptr: *mut PHastPlusWrappedVariant, keys_ptr: *const Box<[Box<[u8]>]>,
                        multiplier: u8, bits_per_seed: u8, bucket_size100: u16, preferred_slice_len: u16, threads_num: usize, ef: bool) {
    let f = unsafe { &mut *struct_ptr };
    let keys = unsafe { &*keys_ptr };
    *f = match (multiplier, bits_per_seed, ef) {
        (1, 8, true) => PHastPlusWrappedVariant::Bits8EF(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<1>
        )),
        (1, _, true) => PHastPlusWrappedVariant::BitsEF(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<1>
        )),

        (1, 8, false) => PHastPlusWrappedVariant::Bits8C(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<1>
        )),
        (1, _, false) => PHastPlusWrappedVariant::BitsC(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &phast::Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<1>
        )),

        (2, 8, true) => PHastPlusWrappedVariant::Bits8EF2(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<2>
        )),
        (2, _, true) => PHastPlusWrappedVariant::BitsEF2(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<2>
        )),

        (2, 8, false) => PHastPlusWrappedVariant::Bits8C2(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<2>
        )),
        (2, _, false) => PHastPlusWrappedVariant::BitsC2(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &phast::Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<2>
        )),

        (3, 8, true) => PHastPlusWrappedVariant::Bits8EF3(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<3>
        )),
        (3, _, true) => PHastPlusWrappedVariant::BitsEF3(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<3>
        )),

        (3, 8, false) => PHastPlusWrappedVariant::Bits8C3(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &Params::new_psl(ph::seeds::Bits8, bucket_size100, preferred_slice_len), threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<3>
        )),
        (3, _, false) => PHastPlusWrappedVariant::BitsC3(phast::Function2::with_slice_p_threads_hash_sc(
            &keys[..], &phast::Params::new_psl(ph::seeds::BitsFast(bits_per_seed), bucket_size100, preferred_slice_len),
            threads_num, seedable_hash::BuildDefaultSeededHasher::default(), ShiftOnlyWrapped::<3>
        )),

        _ => panic!("Wrong multiplier!")
    }
}

#[no_mangle]
pub extern "C" fn queryPhastPlusWrapped(struct_ptr: *const PHastPlusWrappedVariant, key_c_s: *const c_char, length: usize) -> u64 {
    let key = unsafe { slice::from_raw_parts(key_c_s as *const u8, length) };
    match unsafe { &*struct_ptr } {
        PHastPlusWrappedVariant::BitsEF(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8EF(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::BitsC(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8C(function) => function.get(key) as u64,

        PHastPlusWrappedVariant::BitsEF2(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8EF2(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::BitsC2(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8C2(function) => function.get(key) as u64,

        PHastPlusWrappedVariant::BitsEF3(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8EF3(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::BitsC3(function) => function.get(key) as u64,
        PHastPlusWrappedVariant::Bits8C3(function) => function.get(key) as u64,

        PHastPlusWrappedVariant::None => panic!("PHast+ not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn queryPhastPlusWrappedAll(struct_ptr: *const PHastPlusWrappedVariant, keys_ptr: *const Box<[Box<[u8]>]>) {
    let keys = unsafe { &*keys_ptr };
    match unsafe { &*struct_ptr } {
        PHastPlusWrappedVariant::BitsEF(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8EF(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::BitsC(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8C(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },

        PHastPlusWrappedVariant::BitsEF2(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8EF2(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::BitsC2(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8C2(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },

        PHastPlusWrappedVariant::BitsEF3(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8EF3(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::BitsC3(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::Bits8C3(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastPlusWrappedVariant::None => panic!("PHast+ not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn sizePhastPlusWrapped(struct_ptr: *const PHastPlusWrappedVariant) -> usize {
    match unsafe { &*struct_ptr } {
        PHastPlusWrappedVariant::BitsEF(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8EF(function) => function.size_bytes(),
        PHastPlusWrappedVariant::BitsC(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8C(function) => function.size_bytes(),

        PHastPlusWrappedVariant::BitsEF2(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8EF2(function) => function.size_bytes(),
        PHastPlusWrappedVariant::BitsC2(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8C2(function) => function.size_bytes(),

        PHastPlusWrappedVariant::BitsEF3(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8EF3(function) => function.size_bytes(),
        PHastPlusWrappedVariant::BitsC3(function) => function.size_bytes(),
        PHastPlusWrappedVariant::Bits8C3(function) => function.size_bytes(),

        PHastPlusWrappedVariant::None => panic!("PHast+ not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn destroyPhastPlusWrappedStruct(struct_instance: *mut PHastPlusWrappedVariant) {
    unsafe { let _ = Box::from_raw(struct_instance); }
}