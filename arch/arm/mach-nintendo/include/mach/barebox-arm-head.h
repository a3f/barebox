#ifndef __MACH_ARM_HEAD_H
#define __MACH_ARM_HEAD_H

static inline void __barebox_arm_head(void)
{
	__asm__ __volatile__ (
		"b 2f\n"
		".fill  156,1,0\n"		/* Nintendo Logo Character Data (8000004h) */
		".asciz \"barebox\"\n"		/* Game Title */
		".balign 16\n"
		".byte  0x30,0x31\n"		/* Maker Code (80000B0h) */
		".byte  0x96\n"			/* Fixed Value (80000B2h) */
		".byte  0x00\n"			/* Main Unit Code (80000B3h) */
		".byte  0x00\n"			/* Device Type (80000B4h) */
		".fill	7,1,0\n"		/* unused */
		".byte	0x00\n"			/* Software Version No (80000BCh) */
		".byte	0xf0\n"			/* Complement Check (80000BDh) */
		".byte	0x00,0x00\n"   		/* Checksum (80000BEh) */
		".word _text\n"			/* text base. If copied there,
						 * barebox can skip relocation
					 	 */
		".word _barebox_image_size\n"	/* image size to copy */
		".rept 8\n"
		".word 0x55555555\n"
		".endr\n"
		"2:\n"
	);
}

static inline void barebox_arm_head(void)
{
	__barebox_arm_head();
	__asm__ __volatile__ (
		"b barebox_arm_reset_vector\n"
	);
}

#endif /* __MACH_ARM_HEAD_H */
