/*
 * omap iommu: simple virtual address space management
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Written by Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __IOMMU_MMAP_H
#define __IOMMU_MMAP_H

#include <linux/iommu.h>
#include <linux/dma-direction.h>
#include <linux/cdev.h>

#include <plat/dmm_user.h>

#define IOVMM_IOC_MAGIC		'V'

#define IOVMM_IOCSETTLBENT	_IO(IOVMM_IOC_MAGIC, 0)
#define IOVMM_IOCMEMMAP		_IO(IOVMM_IOC_MAGIC, 1)
#define IOVMM_IOCMEMUNMAP	_IO(IOVMM_IOC_MAGIC, 2)
#define IOVMM_IOCDATOPA		_IO(IOVMM_IOC_MAGIC, 3)
#define IOVMM_IOCMEMFLUSH	_IO(IOVMM_IOC_MAGIC, 4)
#define IOVMM_IOCMEMINV		_IO(IOVMM_IOC_MAGIC, 5)
#define IOVMM_IOCCREATEPOOL	_IO(IOVMM_IOC_MAGIC, 6)
#define IOVMM_IOCDELETEPOOL	_IO(IOVMM_IOC_MAGIC, 7)
#define IOVMM_IOCSETPTEENT      _IO(IOVMM_IOC_MAGIC, 8)
#define IOVMM_IOCCLEARPTEENTRIES _IO(IOVMM_IOC_MAGIC, 9)
#define IOMMU_IOCEVENTREG	_IO(IOVMM_IOC_MAGIC, 10)
#define IOMMU_IOCEVENTUNREG	_IO(IOVMM_IOC_MAGIC, 11)

#if 0
struct iovmm_pool {
	u32			pool_id;
	u32			da_begin;
	u32			da_end;
	struct gen_pool		*genpool;
	struct list_head	list;
};

struct iovmm_pool_info {
	u32		pool_id;
	u32		da_begin;
	u32		da_end;
	u32		size;
	u32		flags;
};

/* used to cache dma mapping information */
struct device_dma_map_info {
	/* direction of DMA in action, or DMA_NONE */
	enum dma_data_direction dir;
	/* number of elements requested by us */
	int num_pages;
	/* number of elements returned from dma_map_sg */
	int sg_num;
	/* list of buffers used in this DMA action */
	struct scatterlist *sg;
};

struct dmm_map_info {
	u32 mpu_addr;
	u32 *da;
	u32 num_of_buf;
	u32 size;
	u32  mem_pool_id;
	u32 flags;
};

struct dmm_dma_info {
	void *pva;
	u32 ul_size;
	enum dma_data_direction dir;
};


struct dmm_map_object {
	struct list_head link;
	u32 da;
	u32 va;
	u32 size;
	u32 num_usr_pgs;
	struct gen_pool *gen_pool;
	struct page **pages;
	struct device_dma_map_info dma_info;
};
#endif
struct iodmm_struct {
	struct iovmm_device     *iovmm;
	struct list_head	map_list;
	u32			pool_id;
	spinlock_t		dmm_map_lock;
};

struct iovmm_device {
	/* iommu object which this belongs to */
	struct iommu            *iommu;
	const char              *name;
	/* List of memory pool it manages */
	struct list_head        mmap_pool;
	int                     minor;
	struct cdev cdev;
};

struct iovm_struct {
	struct omap_iommu	*iommu;	/* iommu object which this belongs to */
	const char		*name;
	u32			da_start; /* area definition */
	u32			da_end;
	u32			flags; /* IOVMF_: see below */
	struct list_head	list; /* linked in ascending order */
	const struct sg_table	*sgt; /* keep 'page' <-> 'da' mapping */
	void			*va; /* mpu side mapped address */
};
/*
 * IOVMF_FLAGS: attribute for iommu virtual memory area(iovma)
 *
 * lower 16 bit is used for h/w and upper 16 bit is for s/w.
 */
#define IOVMF_SW_SHIFT		16

/*
 * iovma: h/w flags derived from cam and ram attribute
 */
#define IOVMF_CAM_MASK		(~((1 << 10) - 1))
#define IOVMF_RAM_MASK		(~IOVMF_CAM_MASK)

#define IOVMF_PGSZ_MASK		(3 << 0)
#define IOVMF_PGSZ_1M		MMU_CAM_PGSZ_1M
#define IOVMF_PGSZ_64K		MMU_CAM_PGSZ_64K
#define IOVMF_PGSZ_4K		MMU_CAM_PGSZ_4K
#define IOVMF_PGSZ_16M		MMU_CAM_PGSZ_16M

#define IOVMF_ENDIAN_MASK	(1 << 9)
#define IOVMF_ENDIAN_BIG	MMU_RAM_ENDIAN_BIG
#define IOVMF_ENDIAN_LITTLE	MMU_RAM_ENDIAN_LITTLE

#define IOVMF_ELSZ_MASK		(3 << 7)
#define IOVMF_ELSZ_8		MMU_RAM_ELSZ_8
#define IOVMF_ELSZ_16		MMU_RAM_ELSZ_16
#define IOVMF_ELSZ_32		MMU_RAM_ELSZ_32
#define IOVMF_ELSZ_NONE		MMU_RAM_ELSZ_NONE

#define IOVMF_MIXED_MASK	(1 << 6)
#define IOVMF_MIXED		MMU_RAM_MIXED

/*
 * iovma: s/w flags, used for mapping and umapping internally.
 */
#define IOVMF_MMIO		(1 << IOVMF_SW_SHIFT)
#define IOVMF_ALLOC		(2 << IOVMF_SW_SHIFT)
#define IOVMF_ALLOC_MASK	(3 << IOVMF_SW_SHIFT)

/* "superpages" is supported just with physically linear pages */
#define IOVMF_DISCONT		(1 << (2 + IOVMF_SW_SHIFT))
#define IOVMF_LINEAR		(2 << (2 + IOVMF_SW_SHIFT))
#define IOVMF_LINEAR_MASK	(3 << (2 + IOVMF_SW_SHIFT))

#define IOVMF_DA_FIXED		(1 << (4 + IOVMF_SW_SHIFT))

extern struct iovm_struct *omap_find_iovm_area(struct omap_iommu *obj, u32 da);
extern u32
omap_iommu_vmap(struct iommu_domain *domain, struct omap_iommu *obj, u32 da,
			const struct sg_table *sgt, u32 flags);
extern struct sg_table *omap_iommu_vunmap(struct iommu_domain *domain,
				struct omap_iommu *obj, u32 da);
extern u32
omap_iommu_vmalloc(struct iommu_domain *domain, struct omap_iommu *obj,
				u32 da, size_t bytes, u32 flags);
extern void
omap_iommu_vfree(struct iommu_domain *domain, struct omap_iommu *obj,
				const u32 da);
extern void *omap_da_to_va(struct omap_iommu *obj, u32 da);
extern struct sg_table *iommu_vunmap(struct iommu *obj, u32 da);
extern u32 iommu_vmalloc(struct iommu *obj, u32 da, size_t bytes,
			   u32 flags);
extern void iommu_vfree(struct iommu *obj, const u32 da);

extern void *da_to_va(struct iommu *obj, u32 da);
#endif /* __IOMMU_MMAP_H */
