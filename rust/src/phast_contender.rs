use ph::GetSize;
use ph::phast::{self, compressed_array::CompactFast, DefaultCompressedArray, Params, SeedOnly};
use std::slice;

use std::os::raw::c_char;
use std::hint::black_box;

pub enum PHastVariant {
    BitsEF(phast::Function<ph::seeds::BitsFast, SeedOnly, DefaultCompressedArray>),
    Bits8EF(phast::Function<ph::seeds::Bits8, SeedOnly, DefaultCompressedArray>),
    Bits4EF(phast::Function<ph::seeds::TwoToPowerBitsStatic::<2>, SeedOnly, DefaultCompressedArray>),
    BitsC(phast::Function<ph::seeds::BitsFast, SeedOnly, CompactFast>),
    Bits8C(phast::Function<ph::seeds::Bits8, SeedOnly, CompactFast>),
    Bits4C(phast::Function<ph::seeds::TwoToPowerBitsStatic::<2>, SeedOnly, CompactFast>),
    None
}

#[no_mangle]
pub extern "C" fn createPhastStruct() -> *mut PHastVariant {
    Box::into_raw(Box::new(PHastVariant::None))
}

#[no_mangle]
pub extern "C" fn constructPhast(struct_ptr: *mut PHastVariant, keys_ptr: *const Box<[Box<[u8]>]>,
                                 bits_per_seed: u8, bucket_size100: u16, threads_num: usize, ef: bool) {
    let f = unsafe { &mut *struct_ptr };
    let keys = unsafe { &*keys_ptr };
    *f = match (bits_per_seed, ef) {
        (8, true) => PHastVariant::Bits8EF(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &Params::new(ph::seeds::Bits8, bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),
        (4, true) => PHastVariant::Bits4EF(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &Params::new(ph::seeds::TwoToPowerBitsStatic::<2>, bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),
        (_, true) => PHastVariant::BitsEF(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &Params::new(ph::seeds::BitsFast(bits_per_seed), bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),

        (8, false) => PHastVariant::Bits8C(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &Params::new(ph::seeds::Bits8, bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),
        (4, false) => PHastVariant::Bits4C(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &phast::Params::new(ph::seeds::TwoToPowerBitsStatic::<2>, bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),
        (_, false) => PHastVariant::BitsC(phast::Function::with_slice_p_threads_hash_sc(
            &keys[..],
            &phast::Params::new(ph::seeds::BitsFast(bits_per_seed), bucket_size100),
            threads_num,
            seedable_hash::BuildDefaultSeededHasher::default(),
            SeedOnly
        )),
    }
}

#[no_mangle]
pub extern "C" fn queryPhast(struct_ptr: *const PHastVariant, key_c_s: *const c_char, length: usize) -> u64 {
    let key = unsafe { slice::from_raw_parts(key_c_s as *const u8, length) };
    match unsafe { &*struct_ptr } {
        PHastVariant::BitsEF(function) => function.get(key) as u64,
        PHastVariant::Bits8EF(function) => function.get(key) as u64,
        PHastVariant::Bits4EF(function) => function.get(key) as u64,
        PHastVariant::BitsC(function) => function.get(key) as u64,
        PHastVariant::Bits8C(function) => function.get(key) as u64,
        PHastVariant::Bits4C(function) => function.get(key) as u64,
        PHastVariant::None => panic!("PHast not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn queryPhastAll(struct_ptr: *const PHastVariant, keys_ptr: *const Box<[Box<[u8]>]>) {
    let keys = unsafe { &*keys_ptr };
    match unsafe { &*struct_ptr } {
        PHastVariant::BitsEF(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::Bits8EF(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::Bits4EF(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::BitsC(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::Bits8C(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::Bits4C(function) =>
            for key in keys { black_box(function.get(key.as_ref())); },
        PHastVariant::None => panic!("PHast not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn sizePhast(struct_ptr: *const PHastVariant) -> usize {
    match unsafe { &*struct_ptr } {
        PHastVariant::BitsEF(function) => function.size_bytes(),
        PHastVariant::Bits8EF(function) => function.size_bytes(),
        PHastVariant::Bits4EF(function) => function.size_bytes(),
        PHastVariant::BitsC(function) => function.size_bytes(),
        PHastVariant::Bits8C(function) => function.size_bytes(),
        PHastVariant::Bits4C(function) => function.size_bytes(),
        PHastVariant::None => panic!("PHast not constructed yet"),
    }
}

#[no_mangle]
pub extern "C" fn destroyPhastStruct(struct_instance: *mut PHastVariant) {
    unsafe { let _ = Box::from_raw(struct_instance); }
}
