#ifndef __GBA_BIOS_H_
#define __GBA_BIOS_H_

#include <linux/compiler.h>

enum gba_bios_func {
	BIOS_GBA_SOFT_RESET              = 0x00,
	BIOS_GBA_REGISTER_RAM_RESET      = 0x01,
	BIOS_GBA_HALT                    = 0x02,
	BIOS_GBA_SLEEP                   = 0x03,
	BIOS_GBA_WFI                     = 0x04,
	BIOS_GBA_WFI_VBLANK              = 0x05,
	BIOS_GBA_DIV                     = 0x06,
	BIOS_GBA_DIV_ARM                 = 0x07,
	BIOS_GBA_SQRT                    = 0x08,
	BIOS_GBA_ARCTAN                  = 0x09,
	BIOS_GBA_ARCTAN2                 = 0x0a,
	BIOS_GBA_CPU_SET                 = 0x0b,
	BIOS_GBA_CPU_FASTSET             = 0x0c,
	BIOS_GBA_GET_BIOS_CHECKSUM       = 0x0d,
	BIOS_GBA_BG_AFFINE_SET           = 0x0e,
	BIOS_GBA_OBJ_AFFINE_SET          = 0x0f,
	BIOS_GBA_BIT_UNPACK              = 0x10,
	BIOS_GBA_LZ77_UNCOMP_WRAM        = 0X11,
	BIOS_GBA_LZ77_UNCOMP_VRAM        = 0X12,
	BIOS_GBA_HUFF_UNCOMP             = 0X13,
	BIOS_GBA_RL_UNCOMP_WRAM          = 0X14,
	BIOS_GBA_RL_UNCOMP_VRAM          = 0X15,
	BIOS_GBA_DIFF_8BIT_UNFILTER_WRAM = 0X16,
	BIOS_GBA_DIFF_8BIT_UNFILTER_VRAM = 0X17,
	BIOS_GBA_DIFF_16BIT_UNFILTER     = 0X18,
	BIOS_GBA_SOUND_BIAS              = 0X19,
	BIOS_GBA_SOUND_DRIVER_INIT       = 0X1A,
	BIOS_GBA_SOUND_DRIVER_MODE       = 0X1B,
	BIOS_GBA_SOUND_DRIVER_MAIN       = 0X1C,
	BIOS_GBA_SOUND_DRIVER_VSYNC      = 0X1D,
	BIOS_GBA_SOUND_CHANNEL_CLEAR     = 0X1E,
	BIOS_GBA_MIDIKEY_2FREQ           = 0X1F,
	BIOS_GBA_SOUND_WHATEVER0         = 0X20,
	BIOS_GBA_SOUND_WHATEVER1         = 0X21,
	BIOS_GBA_SOUND_WHATEVER2         = 0X22,
	BIOS_GBA_SOUND_WHATEVER3         = 0X23,
	BIOS_GBA_SOUND_WHATEVER4         = 0X24,
	BIOS_GBA_MULTI_BOOT              = 0X25,
	BIOS_GBA_HARD_RESET              = 0X26,
	BIOS_GBA_CUSTOM_HALT             = 0X27,
	BIOS_GBA_SOUND_DRIVER_VSYNC_OFF  = 0X28,
	BIOS_GBA_SOUND_DRIVER_VSYNC_ON   = 0X29,
	BIOS_GBA_SOUND_GET_JUMPLIST      = 0X2A,
};

static inline int __naked swi(enum gba_bios_func func,
			      int r0, int r1, int r2, int r3)
{
	int ret;
	asm("swi %1" : "=r0"(ret) : "i"(func),
	    "r0"(r0), "r1"(r1), "r2"(r2), "r3"(r3)
	    : "memory");

	return ret;
}

#endif
