/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 * Adapted from arch/arm64/kernel/efi-header.S
 */

#include <linux/pe.h>
#include <asm/image.h>

	.macro	efi_signature_nop
#ifdef CONFIG_EFI_STUB
.L_head:
	/*
	 * This is the encoding of "c.li s4, -13", which has no meaningful
	 * effect except that it forms the magic "MZ" signature required by
	 * UEFI. barebox itself is not built with the C extension, but every
	 * platform running UEFI firmware implements it, so emit the raw
	 * encoding here.
	 */
	.2byte	0x5a4d
#else
	/*
	 * Bootloaders may inspect the opcode at the start of the kernel
	 * image to decide if the kernel is capable of booting via UEFI.
	 * So put an ordinary NOP here, not the "MZ.." pseudo-nop above.
	 */
	nop
#endif
	.endm

	.macro	__EFI_PE_HEADER
#ifdef CONFIG_EFI_STUB
	.set	.Lpe_header_offset, . - .L_head
	.long	PE_MAGIC
#if __riscv_xlen == 64
	.short	IMAGE_FILE_MACHINE_RISCV64		// Machine
#else
	.short	IMAGE_FILE_MACHINE_RISCV32		// Machine
#endif
	.short	.Lsection_count				// NumberOfSections
	.long	0 					// TimeDateStamp
	.long	0					// PointerToSymbolTable
	.long	0					// NumberOfSymbols
	.short	.Lsection_table - .Loptional_header	// SizeOfOptionalHeader
	.short	IMAGE_FILE_DEBUG_STRIPPED | \
		IMAGE_FILE_EXECUTABLE_IMAGE | \
		IMAGE_FILE_LINE_NUMS_STRIPPED		// Characteristics

.Loptional_header:
#if __riscv_xlen == 64
	.short	PE_OPT_MAGIC_PE32PLUS			// PE32+ format
#else
	.short	PE_OPT_MAGIC_PE32			// PE32 format
#endif
	.byte	0x02					// MajorLinkerVersion
	.byte	0x14					// MinorLinkerVersion
	.long	_sdata - .Lefi_header_end		// SizeOfCode
	.long	__pecoff_data_size			// SizeOfInitializedData
	.long	0					// SizeOfUninitializedData
	.long	__efistub_efi_pe_entry - .L_head	// AddressOfEntryPoint
	.long	.Lefi_header_end - .L_head		// BaseOfCode
#if __riscv_xlen == 32
	.long	_sdata - .L_head			// BaseOfData
#endif

#if __riscv_xlen == 64
	.quad	0					// ImageBase
#else
	.long	0					// ImageBase
#endif
	.long	PECOFF_SECTION_ALIGNMENT		// SectionAlignment
	.long	PECOFF_FILE_ALIGNMENT			// FileAlignment
	.short	0					// MajorOperatingSystemVersion
	.short	0					// MinorOperatingSystemVersion
	.short	LINUX_EFISTUB_MAJOR_VERSION		// MajorImageVersion
	.short	LINUX_EFISTUB_MINOR_VERSION		// MinorImageVersion
	.short	0					// MajorSubsystemVersion
	.short	0					// MinorSubsystemVersion
	.long	0					// Win32VersionValue

	.long	__image_end - .L_head			// SizeOfImage

	// Everything before the barebox image is considered part of the header
	.long	.Lefi_header_end - .L_head		// SizeOfHeaders
	.long	0					// CheckSum
	.short	IMAGE_SUBSYSTEM_EFI_APPLICATION		// Subsystem
	.short	0					// DllCharacteristics
#if __riscv_xlen == 64
	.quad	0					// SizeOfStackReserve
	.quad	0					// SizeOfStackCommit
	.quad	0					// SizeOfHeapReserve
	.quad	0					// SizeOfHeapCommit
#else
	.long	0					// SizeOfStackReserve
	.long	0					// SizeOfStackCommit
	.long	0					// SizeOfHeapReserve
	.long	0					// SizeOfHeapCommit
#endif
	.long	0					// LoaderFlags
	.long	(.Lsection_table - .) / 8		// NumberOfRvaAndSizes

	.quad	0					// ExportTable
	.quad	0					// ImportTable
	.quad	0					// ResourceTable
	.quad	0					// ExceptionTable
	.quad	0					// CertificationTable
	.quad	0					// BaseRelocationTable

	// Section table
.Lsection_table:
	.ascii	".text\0\0\0"
	.long	_sdata - .Lefi_header_end		// VirtualSize
	.long	.Lefi_header_end - .L_head		// VirtualAddress
	.long	_sdata - .Lefi_header_end		// SizeOfRawData
	.long	.Lefi_header_end - .L_head		// PointerToRawData

	.long	0					// PointerToRelocations
	.long	0					// PointerToLineNumbers
	.short	0					// NumberOfRelocations
	.short	0					// NumberOfLineNumbers
	.long	IMAGE_SCN_CNT_CODE | \
		IMAGE_SCN_MEM_READ | \
		IMAGE_SCN_MEM_EXECUTE			// Characteristics

	.ascii	".data\0\0\0"
	.long	__pecoff_data_size			// VirtualSize
	.long	_sdata - .L_head			// VirtualAddress
	.long	__pecoff_data_rawsize			// SizeOfRawData
	.long	_sdata - .L_head			// PointerToRawData

	.long	0					// PointerToRelocations
	.long	0					// PointerToLineNumbers
	.short	0					// NumberOfRelocations
	.short	0					// NumberOfLineNumbers
	.long	IMAGE_SCN_CNT_INITIALIZED_DATA | \
		IMAGE_SCN_MEM_READ | \
		IMAGE_SCN_MEM_WRITE			// Characteristics

	.set	.Lsection_count, (. - .Lsection_table) / 40

	.balign	PECOFF_SECTION_ALIGNMENT
.Lefi_header_end:
#else
	.set	.Lpe_header_offset, 0x0
#endif
	.endm
