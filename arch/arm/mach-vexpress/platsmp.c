/*
 *  linux/arch/arm/mach-vexpress/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of_fdt.h>

#include <asm/smp_scu.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>

#include <mach/motherboard.h>

#include "core.h"

extern void versatile_secondary_startup(void);

#if defined(CONFIG_OF)

static enum {
	UNKNOWN_SCU,
	GENERIC_SCU,
	CORTEX_A9_SCU,
} vexpress_dt_scu = UNKNOWN_SCU;

static void __init vexpress_dt_init_cpu_map(int ncores)
{
	int i;

	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
				ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; ++i)
		set_cpu_possible(i, true);
}

static struct map_desc vexpress_dt_cortex_a9_scu_map[] __initdata = {
	{
		.virtual	= (unsigned long)V2T_PERIPH,
		/* .pfn	set in vexpress_dt_init_cortex_a9_scu() */
		.length		= SZ_128,
		.type		= MT_DEVICE,
	},
};

static int __init vexpress_dt_init_cortex_a9_scu(unsigned long node,
		const char *uname, int depth, void *data)
{
	if (of_flat_dt_is_compatible(node, "arm,cortex-a9-scu")) {
		__be32 *reg = of_get_flat_dt_prop(node, "reg", NULL);

		if (WARN_ON(!reg))
			return -EINVAL;

		vexpress_dt_cortex_a9_scu_map[0].pfn =
				__phys_to_pfn(be32_to_cpup(reg));
		iotable_init(vexpress_dt_cortex_a9_scu_map,
				ARRAY_SIZE(vexpress_dt_cortex_a9_scu_map));

		vexpress_dt_init_cpu_map(scu_get_core_count(V2T_PERIPH));
		set_smp_cross_call(gic_raise_softirq);

		return CORTEX_A9_SCU;
	}

	return 0;
}

static int __init vexpress_dt_nr_cpus(unsigned long node, const char *uname,
		int depth, void *data)
{
	static int prev_depth = -1;
	static int nr_cpus = -1;

	if (prev_depth > depth && nr_cpus > 0)
		return nr_cpus;

	if (nr_cpus < 0 && strcmp(uname, "cpus") == 0)
		nr_cpus = 0;

	if (nr_cpus >= 0) {
		const char *device_type = of_get_flat_dt_prop(node,
				"device_type", NULL);

		if (device_type && strcmp(device_type, "cpu") == 0)
			nr_cpus++;
	}

	prev_depth = depth;

	return 0;
}

static void __init vexpress_dt_smp_init_cpus(void)
{
	int ncores;

	vexpress_dt_scu = of_scan_flat_dt(vexpress_dt_init_cortex_a9_scu, NULL);

	if (WARN_ON(vexpress_dt_scu < 0) || vexpress_dt_scu != UNKNOWN_SCU)
		return;

	ncores = of_scan_flat_dt(vexpress_dt_nr_cpus, NULL);
	if (ncores < 2)
		return;

	vexpress_dt_scu = GENERIC_SCU;

	vexpress_dt_init_cpu_map(ncores);
	set_smp_cross_call(gic_raise_softirq);
}

static void __init vexpress_dt_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	switch (vexpress_dt_scu) {
	case GENERIC_SCU:
		for (i = 0; i < max_cpus; i++)
			set_cpu_present(i, true);
		break;
	case CORTEX_A9_SCU:
		scu_enable(V2T_PERIPH);
		break;
	default:
		WARN_ON(1);
		break;
	}
}

#else

static void __init vexpress_dt_smp_init_cpus(void)
{
	WARN_ON(1);
}

void __init vexpress_dt_smp_prepare_cpus(unsigned int max_cpus)
{
	WARN_ON(1);
}

#endif

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	if (ct_desc)
		ct_desc->init_cpu_map();
	else
		vexpress_dt_smp_init_cpus();

}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	if (ct_desc)
		ct_desc->smp_enable(max_cpus);
	else
		vexpress_dt_smp_prepare_cpus(max_cpus);

	/*
	 * Write the address of secondary startup into the
	 * system-wide flags register. The boot monitor waits
	 * until it receives a soft interrupt, and then the
	 * secondary CPU branches to this address.
	 */
	v2m_flags_set(virt_to_phys(versatile_secondary_startup));
}
