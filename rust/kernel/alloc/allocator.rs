// SPDX-License-Identifier: GPL-2.0

//! Allocator support.
//!
//! Documentation for the kernel's memory allocators can found in the "Memory Allocation Guide"
//! linked below. For instance, this includes the concept of "get free page" (GFP) flags and the
//! typical application of the different kernel allocators.
//!
//! Reference: <https://docs.kernel.org/core-api/memory-allocation.html>

use core::alloc::Layout;
use core::ptr;
use core::ptr::NonNull;

use crate::alloc::{AllocError, Allocator};
use crate::bindings;

const ARCH_KMALLOC_MINALIGN: usize = bindings::CONFIG_MALLOC_ALIGNMENT as usize;

/// The contiguous kernel allocator.
///
/// `Kmalloc` is typically used for physically contiguous allocations up to page size, but also
/// supports larger allocations up to `bindings::KMALLOC_MAX_SIZE`, which is hardware specific.
///
/// For more details see [self].
pub struct Kmalloc;

/// # Invariants
///
struct ReallocFunc(
    unsafe extern "C" fn(
        *const crate::ffi::c_void,
        usize,
    ) -> *mut crate::ffi::c_void,
);

impl ReallocFunc {
    // INVARIANT: `realloc` satisfies the type invariants.
    const REALLOC: Self = Self(bindings::realloc);

    /// # Safety
    ///
    /// This method has the same safety requirements as [`Allocator::realloc`].
    ///
    /// # Guarantees
    ///
    /// This method has the same guarantees as `Allocator::realloc`. Additionally
    /// - it accepts any pointer to a valid memory allocation allocated by this function.
    /// - memory allocated by this function remains valid until it is passed to this function.
    #[inline]
    unsafe fn call(
        &self,
        ptr: Option<NonNull<u8>>,
        layout: Layout,
        old_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocError> {
        let size = layout.size();
        let ptr = match ptr {
            Some(ptr) => {
                if old_layout.size() == 0 {
                    ptr::null()
                } else {
                    ptr.as_ptr()
                }
            }
            None => ptr::null(),
        };

        // SAFETY:
        // - `self.0` is one of `realloc` and thus only requires that
        //   `ptr` is NULL or valid.
        // - `ptr` is either NULL or valid by the safety requirements of this function.
        //
        // GUARANTEE:
        // - `self.0` is `realloc`
        // - Those functions provide the guarantees of this function.
        let raw_ptr = unsafe {
            // If `size == 0` and `ptr != NULL` the memory behind the pointer is freed.
            self.0(ptr.cast(), size).cast()
        };

        let ptr = if size == 0 {
            crate::alloc::dangling_from_layout(layout)
        } else {
            NonNull::new(raw_ptr).ok_or(AllocError)?
        };

        Ok(NonNull::slice_from_raw_parts(ptr, size))
    }
}

impl Kmalloc {
    /// Returns a [`Layout`] that makes [`Kmalloc`] fulfill the requested size and alignment of
    /// `layout`.
    pub fn aligned_layout(layout: Layout) -> Layout {
        // Note that `layout.size()` (after padding) is guaranteed to be a multiple of
        // `layout.align()` which together with the slab guarantees means that `Kmalloc` will return
        // a properly aligned object (see comments in `kmalloc()` for more information).
        layout.pad_to_align()
    }
}

// SAFETY: `realloc` delegates to `ReallocFunc::call`, which guarantees that
// - memory remains valid until it is explicitly freed,
// - passing a pointer to a valid memory allocation is OK,
// - `realloc` satisfies the guarantees, since `ReallocFunc::call` has the same.
unsafe impl Allocator for Kmalloc {
    const MIN_ALIGN: usize = ARCH_KMALLOC_MINALIGN;

    #[inline]
    unsafe fn realloc(
        ptr: Option<NonNull<u8>>,
        layout: Layout,
        old_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocError> {
        let layout = Kmalloc::aligned_layout(layout);

        // SAFETY: `ReallocFunc::call` has the same safety requirements as `Allocator::realloc`.
        unsafe { ReallocFunc::REALLOC.call(ptr, layout, old_layout) }
    }
}
