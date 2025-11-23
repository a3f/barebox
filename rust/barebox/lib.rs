// SPDX-License-Identifier: GPL-2.0

//! The `barebox` crate.
//!
//! This crate contains the barebox APIs that have been ported or wrapped for
//! usage by Rust code in the barebox and is shared by all of them.

#![no_std]

use kernel::{
    prelude::*,
};

/// Prefix to appear before log messages printed from within the `barebox` crate.
const __LOG_PREFIX: &[u8] = b"rust_barebox\0";

#[export]
unsafe extern "C" fn rust_print_hellow() -> () {
    kernel::pr_notice!("Hello, {}!\n", "world");
}
