/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer
 * Author: Martin Persson
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 */

#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <mach/id.h>

static struct cpufreq_frequency_table db8500_freq_table[] = {
	[0] = {
		.index = 0,
		.frequency = 200000,
	},
	[1] = {
		.index = 1,
		.frequency = 300000,
	},
	[2] = {
		.index = 2,
		.frequency = 600000,
	},
	[3] = {
		/* Used for MAX_OPP, if available */
		.index = 3,
		.frequency = CPUFREQ_TABLE_END,
	},
	[4] = {
		.index = 4,
		.frequency = CPUFREQ_TABLE_END,
	},
};

static struct cpufreq_frequency_table db5500_freq_table[] = {
	[0] = {
		.index = 0,
		.frequency = 200000,
	},
	[1] = {
		.index = 1,
		.frequency = 396500,
	},
	[2] = {
		.index = 2,
		.frequency = 793000,
	},
	[3] = {
		.index = 3,
		.frequency = CPUFREQ_TABLE_END,
	},
};

static struct cpufreq_frequency_table *freq_table;

static enum arm_opp db8500_idx2opp[] = {
	ARM_EXTCLK,
	ARM_50_OPP,
	ARM_100_OPP,
	ARM_MAX_OPP
};

static enum arm_opp db5500_idx2opp[] = {
	ARM_EXTCLK,
	ARM_50_OPP,
	ARM_100_OPP,
};

static enum arm_opp *idx2opp;

static struct freq_attr *dbx500_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int dbx500_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static int dbx500_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	struct cpufreq_freqs freqs;
	unsigned int idx;

	/* scale the target frequency to one of the extremes supported */
	if (target_freq < policy->cpuinfo.min_freq)
		target_freq = policy->cpuinfo.min_freq;
	if (target_freq > policy->cpuinfo.max_freq)
		target_freq = policy->cpuinfo.max_freq;

	/* Lookup the next frequency */
	if (cpufreq_frequency_table_target
	    (policy, freq_table, target_freq, relation, &idx)) {
		return -EINVAL;
	}

	freqs.old = policy->cur;
	freqs.new = freq_table[idx].frequency;

	if (freqs.old == freqs.new)
		return 0;

	/* pre-change notification */
	for_each_cpu(freqs.cpu, policy->cpus)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* request the PRCM unit for opp change */
	if (prcmu_set_arm_opp(idx2opp[idx])) {
		pr_err("ux500-cpufreq:  Failed to set OPP level\n");
		return -EINVAL;
	}

	/* post change notification */
	for_each_cpu(freqs.cpu, policy->cpus)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static unsigned int dbx500_cpufreq_getspeed(unsigned int cpu)
{
	int i;
	/* request the prcm to get the current ARM opp */
	for (i = 0; prcmu_get_arm_opp() != idx2opp[i]; i++)
		;
	return freq_table[i].frequency;
}

static bool initialized;

static void __init dbx500_cpufreq_early_init(void)
{
	if (cpu_is_u5500()) {
		freq_table = db5500_freq_table;
		idx2opp = db5500_idx2opp;

	} else if (cpu_is_u8500()) {
		freq_table = db8500_freq_table;
		idx2opp = db8500_idx2opp;

		if (!prcmu_is_u8400()) {
			freq_table[1].frequency = 400000;
			freq_table[2].frequency = 800000;
			if (prcmu_has_arm_maxopp())
				freq_table[3].frequency = 1000000;
		}

	} else {
		ux500_unknown_soc();
	}

	initialized = true;
}

/*
 * This is called from localtimer initialization, via the clk_get_rate() for
 * the smp_twd clock.  This is way before cpufreq is initialized.
 */
unsigned long dbx500_cpufreq_getfreq(void)
{
	if (!initialized)
		dbx500_cpufreq_early_init();

	return dbx500_cpufreq_getspeed(0) * 1000;
}

int dbx500_cpufreq_get_limits(int cpu, int r,
			      unsigned int *min, unsigned int *max)
{
	int op;
	int i;
	int ret;
	static int old_freq;
	struct cpufreq_policy p;

	switch (r) {
	case 0:
		/* Fall through */
	case 25:
		op = ARM_EXTCLK;
		break;
	case 50:
		op = ARM_50_OPP;
		break;
	case 100:
		op = ARM_100_OPP;
		break;
	case 125:
		if (cpu_is_u8500() && prcmu_has_arm_maxopp())
			op = ARM_MAX_OPP;
		else
			op = ARM_100_OPP;
		break;
	default:
		pr_err("cpufreq-dbx500: Incorrect arm target value (%d).\n",
		       r);
		BUG();
		break;
	}

	for (i = 0; idx2opp[i] != op; i++)
		;

	if (freq_table[i].frequency == CPUFREQ_TABLE_END) {
		pr_err("cpufreq-dbx500: Minimum frequency does not exist!\n");
		BUG();
	}

	if (freq_table[i].frequency != old_freq)
		pr_debug("cpufreq-dbx500: set min arm freq to %d\n",
			 freq_table[i].frequency);

	(*min) = freq_table[i].frequency;

	ret = cpufreq_get_policy(&p, cpu);
	if (ret) {
		pr_err("cpufreq-dbx500: Failed to get policy.\n");
		return -EINVAL;
	}

	(*max) = p.max;
	return 0;
}

static int __cpuinit dbx500_cpufreq_init(struct cpufreq_policy *policy)
{
	int res;
	int i;

	/* get policy fields based on the table */
	res = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (!res)
		cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	else {
		pr_err("dbx500-cpufreq : Failed to read policy table\n");
		return res;
	}

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = dbx500_cpufreq_getspeed(policy->cpu);

	for (i = 0; freq_table[i].frequency != policy->cur; i++)
		;

	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	/*
	 * FIXME : Need to take time measurement across the target()
	 *	   function with no/some/all drivers in the notification
	 *	   list.
	 */
	policy->cpuinfo.transition_latency = 20 * 1000; /* in ns */

	/* policy sharing between dual CPUs */
	cpumask_copy(policy->cpus, &cpu_present_map);

	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;

	return 0;
}

static struct cpufreq_driver dbx500_cpufreq_driver = {
	.flags  = CPUFREQ_STICKY,
	.verify = dbx500_cpufreq_verify_speed,
	.target = dbx500_cpufreq_target,
	.get    = dbx500_cpufreq_getspeed,
	.init   = dbx500_cpufreq_init,
	.name   = "DBX500",
	.attr   = dbx500_cpufreq_attr,
};

static int __init dbx500_cpufreq_register(void)
{
	int i;

	if (cpu_is_u5500() && cpu_is_u5500v1())
		return -ENODEV;

	if (cpu_is_u8500() && !cpu_is_u8500v20_or_later())
		return -ENODEV;

	if (!initialized)
		dbx500_cpufreq_early_init();

	pr_info("dbx500-cpufreq : Available frequencies:\n");

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		pr_info("  %d Mhz\n", freq_table[i].frequency / 1000);

	return cpufreq_register_driver(&dbx500_cpufreq_driver);
}
device_initcall(dbx500_cpufreq_register);
