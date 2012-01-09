/*
 * tiler_pack.c
 *
 * TILER driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <mach/tiler.h>
#include "tiler-def.h"
#include "_tiler.h"

/* we want to find the most effective packing for the smallest area */

/* we have two algorithms for packing nv12 blocks */

/* we want to find the most effective packing for the smallest area */

static inline u32 nv12_eff(u16 n, u16 w, u16 area, u16 n_need)
{
	/* rank by total area needed first */
	return 0x10000000 - DIV_ROUND_UP(n_need, n) * area * 32 +
		/* then by efficiency */
		1024 * n * ((w * 3 + 1) >> 1) / area;
}

/* This method is used for both 2D and NV12 packing */

/* return maximum buffers that can be packed next to each other */
/* o(ffset), w(idth), e(ff_width), b(and), n(um blocks), area( needed) */
/* assumptions: w > 0, o < a <= e */
static u32 tiler_best2pack(u16 o, u16 w, u16 e, u16 b, u16 *n, u16 *area)
{
	u16 m = 0, max_n = *n;		/* m is mostly n - 1 */
	u32 eff, best_eff = 0;		/* best values */
	u16 stride = ALIGN(o + w, b), ar = stride;	/* current area */

	/*
	 * blocks must fit in tiler container and
	 * block stride must be the same: defined as align(o + w, b)
	 *
	 * == align(o + (n-1) * e + w, b) - trim((o + (n-1) * e, b) for all n
	 */
	while (m < max_n &&
	       o + m * e + w <= TILER_WIDTH &&
	       stride == ALIGN(ar - o - m * e, b)) {
		/* get efficiency */
		m++;
		eff = m * w * 1024 / ar;
		if (eff > best_eff) {
			best_eff = eff;
			*n = m;
			if (area)
				*area = ar;
		}
		ar = ALIGN(o + m * e + w, b);
	}

	return best_eff;
}

/* We have two algorithms for packing nv12 blocks: either pack 8 and 16 bit
   blocks separately as 2D, or pack them into same area */

/* nv12 packing algorithm 1: pack 8 and 16 bit block into separate areas */
/* assumptions: w > 0, o < a, 2 <= a */
static u16 nv12_separate(u16 o, u16 w, u16 a, u16 n, u16 *area)
{
	tiler_best2pack(o, w, ALIGN(w, a), 64, &n, area);
	tiler_best2pack(o / 2, (w + 1) / 2, ALIGN(w, a) / 2, 32, &n, area);
	*area *= 3;
	return n;
}

/* We use 4 packing methods for same area packing that give the best result
   for most parameters.  We pack into a 64-slot area, so that we don't have
   to worry about stride issues (all blocks get 4K stride).  For some of the
   algorithms this could be true even if the area was 128. */

/* packing types are marked using a letter sequence, capital letters denoting
   8-bit blocks, lower case letters denoting corresponding 16-bit blocks. */

/* progressive packing: AAAAaaaaBBbbCc into 64-slot area */
/* o(ffset), w(idth), a(lign), area, n(um blocks), p(acking) */
#define MAX_A 21
static int nv12_A(u16 o, u16 w, u16 a, u16 *area, u16 n, u8 *p)
{
	u16 x = o, u, l, m = 0;
	*area = 64;

	while (x + w < *area && m < n) {
		/* current 8bit upper bound (a) is next 8bit lower bound (B) */
		l = u = (*area + x) >> 1;

		/* pack until upper bound */
		while (x + w <= u && m < n) {
			/* save packing */
			*p++ = x;
			*p++ = l;
			l = (*area + x + w + 1) >> 1;
			x = ALIGN(x + w - o, a) + o;
			m++;
		}
		x = ALIGN(l - o, a) + o;	/* set new lower bound */
	}
	return m;
}

/* regressive packing: cCbbBBaaaaAAAA into 64-slot area */
static int nv12_revA(u16 o, u16 w, u16 a, u16 *area, u16 n, u8 *p)
{
	u16 m;
	n = nv12_A((a - (o + w) % a) % a, w, a, area, n, p);
	/* reverse packing */
	for (m = 0; m < n; m++) {
		*p = *area - *p - w;
		p++;
		*p = *area - *p - ((w + 1) >> 1);
		p++;
	}
	return n;
}

/* simple layout: aAbcBdeCfgDhEFGH */
#define MAX_B 8
static int nv12_B(u16 o, u16 w, u16 a, u16 *area, u16 n, u8 *p)
{
	u16 e  = (o + w) % a;	/* end offset */
	u16 o1 = (o >> 1) % a;			/* half offset */
	u16 e1 = ((o + w + 1) >> 1) % a;	/* half end offset */
	u16 o2 = o1 + (a >> 2);			/* 2nd half offset */
	u16 e2 = e1 + (a >> 2);			/* 2nd half end offset */
	u16 m = 0;
	*area = 64;

	/* ensure 16-bit blocks don't overlap 8-bit blocks */

	/* width cannot wrap around alignment, half block must be before block,
	   2nd half can be before or after */
	if (w < a && o < e && e1 <= o && (e2 <= o || o2 >= e))
		while (o + w <= *area && m < n) {
			*p++ = o;
			*p++ = o >> 1;
			m++;
			o += a;
		}
	return m;
}

/* butterfly layout: AAbbaaBB */
#define MAX_C 20
static int nv12_C(u16 o, u16 w, u16 a, u16 *area, u16 n, u8 *p)
{
	int m = 0;
	u16 o2, e = ALIGN(w, a), i = 0, j = 0;
	*area = 64;
	o2 = *area - (a - (o + w) % a) % a;	/* end of last possible block */

	m = (min(o2 - 2 * o, 2 * o2 - o - *area) / 3 - w) / e + 1;
	for (i = j = 0; i < m && j < n; i++, j++) {
		*p++ = o + i * e;
		*p++ = (o + i * e + *area) >> 1;
		if (++j < n) {
			*p++ = o2 - i * e - w;
			*p++ = (o2 - i * e - w) >> 1;
		}
	}
	return j;
}

/* for large allocation: aA or Aa */
#define MAX_D 1
static int nv12_D(u16 o, u16 w, u16 a, u16 *area, u16 n, u8 *p)
{
	u16 o1, w1 = (w + 1) >> 1, d;
	*area = ALIGN(o + w, 64);

	for (d = 0; n > 0 && d + o + w <= *area; d += a) {
		/* fit 16-bit before 8-bit */
		o1 = ((o + d) % 64) >> 1;
		if (o1 + w1 <= o + d) {
			*p++ = o + d;
			*p++ = o1;
			return 1;
		}

		/* fit 16-bit after 8-bit */
		o1 += ALIGN(d + o + w - o1, 32);
		if (o1 + w1 <= *area) {
			*p++ = o;
			*p++ = o1;
			return 1;
		}
	}
	return 0;
}

#define MAX_ANY max(max(MAX_A, MAX_B), max(MAX_C, MAX_D))

/* nv12 packing algorithm 2: pack 8 and 16 bit block into same areas */
/* assumptions: w > 0, o < a, 2 <= a, packing has at least MAX_ANY * 2 bytes */
static u16 nv12_together(u16 o, u16 w, u16 a, u16 n, u16 *area, u8 *packing)
{
	u16 n_best, n2, a_, o_, w_;

	/* algo results (packings) */
	u8 pack_A[MAX_A * 2], pack_rA[MAX_A * 2];
	u8 pack_B[MAX_B * 2], pack_C[MAX_C * 2];
	u8 pack_D[MAX_D * 2];

	/* These packings are sorted by increasing area, and then by decreasing
	   n.  We may not get the best efficiency as we are trying to minimize
	   the area. */
	u8 packings[] = {
		/* n=9, o=2, w=4, a=4, area=64 */
		9, 2, 4, 4, 64,
			2, 33,	6, 35,	10, 37,	14, 39,	18, 41,
			46, 23,	50, 25, 54, 27,	58, 29,
		/* o=0, w=12, a=4, n=3 */
		3, 0, 12, 4, 64,
			0, 32,	12, 38,	48, 24,
		/* end */
		0
	}, *p = packings, *p_best = NULL, *p_end;
	p_end = packings + sizeof(packings) - 1;

	/* see which method gives the best packing */

	/* start with smallest area algorithms A, B & C, stop if we can
	   pack all buffers */
	n_best = nv12_A(o, w, a, area, n, pack_A);
	p_best = pack_A;
	if (n_best < n) {
		n2 = nv12_revA(o, w, a, area, n, pack_rA);
		if (n2 > n_best) {
			n_best = n2;
			p_best = pack_rA;
		}
	}
	if (n_best < n) {
		n2 = nv12_B(o, w, a, area, n, pack_B);
		if (n2 > n_best) {
			n_best = n2;
			p_best = pack_B;
		}
	}
	if (n_best < n) {
		n2 = nv12_C(o, w, a, area, n, pack_C);
		if (n2 > n_best) {
			n_best = n2;
			p_best = pack_C;
		}
	}

	/* traverse any special packings */
	while (*p) {
		n2 = *p++;
		o_ = *p++;
		w_ = *p++;
		a_ = *p++;
		/* stop if we already have a better packing */
		if (n2 < n_best)
			p = p_end;	/* fake stop */

		/* check if this packing is satisfactory */
		else if (a_ >= a && o + w + ALIGN(o_ - o, a) <= o_ + w_) {
			*area = *p++;
			n_best = min(n2, n);
			p_best = p;
			break;
		} else {
			/* skip to next packing */
			p += 1 + n2 * 2;
		}
	}

	/* check whether 8 and 16 bit blocks can be co-packed (this will
	   actually be done in the end by the normal allocation) to see if
	   this is just as good as packing separately */
	if (!n_best) {
		n_best = nv12_D(o, w, a, area, n, pack_D);
		p_best = NULL;
	}

	if (p_best && n_best)
		memcpy(packing, p_best, n_best * 2 * sizeof(*pack_A));

	return n_best;
}

/* can_together: 8-bit and 16-bit views are in the same container */
void reserve_nv12(u32 n, u32 width, u32 height, u32 align, u32 offs,
			 u32 gid, struct process_info *pi, bool can_together)
{
	/* adjust alignment to at least 128 bytes (16-bit slot width) */
	u16 w, h, band, a = MAX(128, align), o = offs, eff_w;
	struct gid_info *gi;
	int res = 0, res2, i;
	u16 n_t, n_s, area_t, area_s;
	u8 packing[2 * 21];
	struct list_head reserved = LIST_HEAD_INIT(reserved);

	/* Check input parameters for correctness, and support */
	if (!width || !height || !n ||
	    offs >= align || offs & 1 ||
	    align >= PAGE_SIZE ||
	    n > TILER_WIDTH * TILER_HEIGHT / 2)
		return;

	/* calculate dimensions, band, offs and alignment in slots */
	if (__analize_area(TILFMT_8BIT, width, height, &w, &h, &band, &a, &o,
									NULL))
		return;

	/* get group context */
	gi = get_gi(pi, gid);
	if (!gi)
		return;

	eff_w = ALIGN(w, a);

	for (i = 0; i < n && res >= 0; i += res) {
		/* check packing separately vs together */
		n_s = nv12_separate(o, w, a, n - i, &area_s);
		if (can_together)
			n_t = nv12_together(o, w, a, n - i, &area_t, packing);
		else
			n_t = 0;

		/* pack based on better efficiency */
		res = -1;
		if (!can_together ||
			nv12_eff(n_s, w, area_s, n - i) >
			nv12_eff(n_t, w, area_t, n - i)) {

			/* reserve blocks separately into a temporary list,
			   so that we can free them if unsuccessful */
			res = reserve_2d(TILFMT_8BIT, n_s, w, h, band, a, o, gi,
					 &reserved);

			/* only reserve 16-bit blocks if 8-bit was successful,
			   as we will try to match 16-bit areas to an already
			   reserved 8-bit area, and there is no guarantee that
			   an unreserved 8-bit area will match the offset of
			   a singly reserved 16-bit area. */
			res2 = (res < 0 ? res :
				reserve_2d(TILFMT_16BIT, n_s, (w + 1) / 2, h,
				band / 2, a / 2, o / 2, gi, &reserved));
			if (res2 < 0 || res != res2) {
				/* clean up */
				release_blocks(&reserved);
				res = -1;
			} else {
				/* add list to reserved */
				add_reserved_blocks(&reserved, gi);
			}
		}

		/* if separate packing failed, still try to pack together */
		if (res < 0 && can_together && n_t) {
			/* pack together */
			res = pack_nv12(n_t, area_t, w, h, gi, packing);
		}
	}

	release_gi(gi);
}

/* reserve 2d blocks (if standard allocator is inefficient) */
void reserve_blocks(u32 n, enum tiler_fmt fmt, u32 width, u32 height,
			   u32 align, u32 offs, u32 gid,
			   struct process_info *pi)
{
	u32 til_width, bpp, bpt, res = 0, i;
	u16 o = offs, a = align, band, w, h, e, n_try;
	struct gid_info *gi;

	/* Check input parameters for correctness, and support */
	if (!width || !height || !n ||
	    align > PAGE_SIZE || offs >= align ||
	    fmt < TILFMT_8BIT || fmt > TILFMT_32BIT)
		return;

	/* tiler page width in pixels, bytes per pixel, tiler page in bytes */
	til_width = fmt == TILFMT_32BIT ? 32 : 64;
	bpp = 1 << (fmt - TILFMT_8BIT);
	bpt = til_width * bpp;

	/* check offset.  Also, if block is less than half the mapping window,
	   the default allocation is sufficient.  Also check for basic area
	   info. */
	if (width * bpp * 2 <= PAGE_SIZE ||
	    __analize_area(fmt, width, height, &w, &h, &band, &a, &o, NULL))
		return;

	/* get group id */
	gi = get_gi(pi, gid);
	if (!gi)
		return;

	/* effective width of a buffer */
	e = ALIGN(w, a);

	for (i = 0; i < n && res >= 0; i += res) {
		/* blocks to allocate in one area */
		n_try = MIN(n - i, TILER_WIDTH);
		tiler_best2pack(offs, w, e, band, &n_try, NULL);

		res = -1;
		while (n_try > 1) {
			res = reserve_2d(fmt, n_try, w, h, band, a, o, gi,
					 &gi->reserved);
			if (res >= 0)
				break;

			/* reduce n if failed to allocate area */
			n_try--;
		}
	}
	/* keep reserved blocks even if failed to reserve all */

	release_gi(gi);
}

void unreserve_blocks(struct process_info *pi, u32 gid)
{
	struct gid_info *gi;

	gi = get_gi(pi, gid);
	if (!gi)
		return;

	release_blocks(&gi->reserved);

	release_gi(gi);
}
