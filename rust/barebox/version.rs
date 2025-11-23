// SPDX-License-Identifier: GPL-2.0

use kernel::{
    prelude::*,
    print
};

#[export]
unsafe extern "C" fn rust_print_hellow() -> () {
    pr_info!("Hello, {}!\n", "world")
}
