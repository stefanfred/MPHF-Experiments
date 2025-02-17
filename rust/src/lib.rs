#![feature(isqrt)]

extern crate libc;
extern crate ph;
extern crate rayon;

use std::os::raw::c_char;
use libc::strlen;
use std::slice;
use std::str;
use ph::{fmph, GetSize};
use ph::fmph::{BuildConf, GOBuildConf};
use mem_dbg::SizeFlags;
use ptr_hash::PtrHashParams;

////////////////////////////////////////// General methods //////////////////////////////////////////
fn c_strings_to_vec(len: usize, my_strings: *const *const c_char) -> Vec<String> {
    let mut vector = Vec::new();
    let sl = unsafe { std::slice::from_raw_parts(my_strings, len) };
    let mut index = 0;
    while index < sl.len() {
        let c_s = sl[index];
        let s = unsafe { str::from_utf8_unchecked(slice::from_raw_parts(c_s as *const u8, strlen(c_s)+1)) };
        vector.push(String::from(s));
        index += 1;
    }
    return vector;
}

fn c_strings_to_slices(len: usize, my_strings: *const *const c_char) -> Vec<&'static [u8]> {
    let mut vector = Vec::new();
    let sl = unsafe { std::slice::from_raw_parts(my_strings, len) };
    let mut index = 0;
    while index < sl.len() {
        let c_s = sl[index];
        let s = unsafe { slice::from_raw_parts(c_s as *const u8, strlen(c_s) + 1) };
        vector.push(s);
        index += 1;
    }
    return vector;
}

#[no_mangle]
pub extern "C" fn initializeRayonThreadPool(threads: usize) {
    rayon::ThreadPoolBuilder::new().num_threads(threads).build_global().unwrap();
}

////////////////////////////////////////// Fmph //////////////////////////////////////////
pub struct FmphWrapper {
    vector: Vec<String>,
    hash_func: fmph::Function
}

#[no_mangle]
pub extern "C" fn createFmphStruct(len: usize, my_strings: *const *const c_char) -> *mut FmphWrapper {
    let struct_instance = FmphWrapper {
        vector: c_strings_to_vec(len, my_strings),
        hash_func: fmph::Function::from(&[] as &[String]) };
    let boxx = Box::new(struct_instance);
    Box::into_raw(boxx)
}

#[no_mangle]
pub extern "C" fn constructFmph(struct_ptr: *mut FmphWrapper, gamma : u16) {
    let struct_instance = unsafe { &mut *struct_ptr };
    let mut build_config = BuildConf::default();
    build_config.use_multiple_threads = true;
    build_config.relative_level_size = gamma;
    struct_instance.hash_func = fmph::Function::from_slice_with_conf(&struct_instance.vector[..], build_config);
}

#[no_mangle]
pub extern "C" fn queryFmph(struct_ptr: *mut FmphWrapper, key_c_s : *const c_char, length : usize) -> u64 {
    let struct_instance = unsafe { &mut *struct_ptr };
    let key = unsafe { str::from_utf8_unchecked(slice::from_raw_parts(key_c_s as *const u8, length+1)) };
    struct_instance.hash_func.get(key).unwrap()
}

#[no_mangle]
pub extern "C" fn sizeFmph(struct_ptr: *mut FmphWrapper) -> usize {
    let struct_instance = unsafe { &mut *struct_ptr };
    struct_instance.hash_func.size_bytes()
}

#[no_mangle]
pub extern "C" fn destroyFmphStruct(struct_instance: *mut FmphWrapper) {
    unsafe { let _ = Box::from_raw(struct_instance); }
}

////////////////////////////////////////// FmphGO //////////////////////////////////////////
pub struct FmphGoWrapper {
    vector: Vec<String>,
    hash_func: fmph::GOFunction
}

#[no_mangle]
pub extern "C" fn createFmphGoStruct(len: usize, my_strings: *const *const c_char) -> *mut FmphGoWrapper {
    let struct_instance = FmphGoWrapper {
        vector: c_strings_to_vec(len, my_strings),
        hash_func: fmph::GOFunction::from(&[] as &[String]) };
    let boxx = Box::new(struct_instance);
    Box::into_raw(boxx)
}

#[no_mangle]
pub extern "C" fn constructFmphGo(struct_ptr: *mut FmphGoWrapper, gamma : u16) {
    let struct_instance = unsafe { &mut *struct_ptr };
    let mut build_config = GOBuildConf::default();
    build_config.use_multiple_threads = true;
    build_config.relative_level_size = gamma;
    struct_instance.hash_func = fmph::GOFunction::from_slice_with_conf(&struct_instance.vector[..], build_config);
}

#[no_mangle]
pub extern "C" fn queryFmphGo(struct_ptr: *mut FmphGoWrapper, key_c_s : *const c_char, length : usize) -> u64 {
    let struct_instance = unsafe { &mut *struct_ptr };
    let key = unsafe { str::from_utf8_unchecked(slice::from_raw_parts(key_c_s as *const u8, length+1)) };
    struct_instance.hash_func.get(key).unwrap()
}

#[no_mangle]
pub extern "C" fn sizeFmphGo(struct_ptr: *mut FmphGoWrapper) -> usize {
    let struct_instance = unsafe { &mut *struct_ptr };
    struct_instance.hash_func.size_bytes()
}

#[no_mangle]
pub extern "C" fn destroyFmphGoStruct(struct_instance: *mut FmphGoWrapper) {
    unsafe { let _ = Box::from_raw(struct_instance); }
}

//////////////////////////////////////////// PtrHash ///////////////////////////////////////////
type PtrHashFast = ptr_hash::PtrHash<&'static [u8], ptr_hash::bucket_fn::Linear, Vec<u32>, ptr_hash::hash::Xx64, Vec<u8>>;
type PtrHashCompact = ptr_hash::PtrHash<&'static [u8], ptr_hash::bucket_fn::CubicEps,
    ptr_hash::CachelineEfVec, ptr_hash::hash::Xx64, Vec<u8>>;
pub struct PtrHashWrapper {
    vector: Vec<&'static [u8]>,
    compact: bool,
    ptr_hash_fast: PtrHashFast,
    ptr_hash_compact: PtrHashCompact,
}

#[no_mangle]
pub extern "C" fn createPtrHashStruct(len: usize, my_strings: *const *const c_char) -> *mut PtrHashWrapper {
    let struct_instance = PtrHashWrapper {
        vector: c_strings_to_slices(len, my_strings),
        compact: false,
        ptr_hash_fast: PtrHashFast::default(),
        ptr_hash_compact: PtrHashCompact::default() };
    let boxx = Box::new(struct_instance);
    Box::into_raw(boxx)
}

#[no_mangle]
pub extern "C" fn constructPtrHash(struct_ptr: *mut PtrHashWrapper, compact : bool) {
    let struct_instance = unsafe { &mut *struct_ptr };
    struct_instance.compact = compact;
    if compact {
        struct_instance.ptr_hash_compact =
            PtrHashCompact::new(&struct_instance.vector[..], PtrHashParams::default());
    } else {
        struct_instance.ptr_hash_fast =
            PtrHashFast::new(&struct_instance.vector[..], PtrHashParams::default());
    }
}

#[no_mangle]
pub extern "C" fn queryPtrHash(struct_ptr: *mut PtrHashWrapper, key_c_s : *const c_char, length : usize) -> u64 {
    let struct_instance = unsafe { &mut *struct_ptr };
    let key = unsafe { slice::from_raw_parts(key_c_s as *const u8, length + 1) };
    if struct_instance.compact {
        struct_instance.ptr_hash_compact.index_minimal(&key) as u64
    } else {
        struct_instance.ptr_hash_fast.index_minimal(&key) as u64
    }
}

#[no_mangle]
pub extern "C" fn sizePtrHash(struct_ptr: *mut PtrHashWrapper) -> usize {
    let struct_instance = unsafe { &mut *struct_ptr };
    use mem_dbg::MemSize;
    return if struct_instance.compact {
        struct_instance.ptr_hash_compact.mem_size(SizeFlags::default())
    } else {
        struct_instance.ptr_hash_fast.mem_size(SizeFlags::default())
    }
}

#[no_mangle]
pub extern "C" fn destroyPtrHashStruct(struct_instance: *mut PtrHashWrapper) {
    unsafe { let _ = Box::from_raw(struct_instance); }
}
