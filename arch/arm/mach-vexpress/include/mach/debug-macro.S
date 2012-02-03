/* arch/arm/mach-realview/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG_LL_PHYS_BASE		0x10000000
#define DEBUG_LL_UART_OFFSET		0x00009000

#define DEBUG_LL_PHYS_BASE_RS1		0x1c000000
#define DEBUG_LL_UART_OFFSET_RS1	0x00090000

#define DEBUG_LL_VIRT_BASE		0xf8000000

		.macro	addruart,rp,rv,tmp

		@ Check the MMU state
#if defined(CONFIG_MMU)
		mrc	p15, 0, \tmp, c1, c0	@ SCTRL
		tst	\tmp, #1		@ MMU enabled?
		moveq	\tmp, #DEBUG_LL_PHYS_BASE
		movne	\tmp, #DEBUG_LL_VIRT_BASE
#else
		mov	\tmp, #DEBUG_LL_PHYS_BASE
#endif

		@ PL011 present in "original" place?
		orr	\tmp, \tmp, #DEBUG_LL_UART_OFFSET
		ldr	\tmp, [\tmp, #0xfe0]	@ PeriphID0
		teq	\tmp, #0x11		@ PL011

		@ Original memory map
		moveq	\rp, #DEBUG_LL_UART_OFFSET
		orreq	\rv, \rp, #DEBUG_LL_VIRT_BASE
		orreq	\rp, \rp, #DEBUG_LL_PHYS_BASE

		@ RS1 memory map
		movne	\rp, #DEBUG_LL_UART_OFFSET_RS1
		orrne	\rv, \rp, #DEBUG_LL_VIRT_BASE
		orrne	\rp, \rp, #DEBUG_LL_PHYS_BASE_RS1

		.endm

#include <asm/hardware/debug-pl01x.S>
