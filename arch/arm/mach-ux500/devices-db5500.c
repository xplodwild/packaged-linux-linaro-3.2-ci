/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 *
 * Author: Pierre Peiffer <pierre.peiffer@stericsson.com> for ST-Ericsson.
 * for the System Trace Module part.
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio/nomadik.h>

#include <mach/hardware.h>
#include <mach/devices.h>

#ifdef CONFIG_FB_MCDE
#include <video/mcde.h>
#endif
#include <mach/db5500-regs.h>

#include <linux/mfd/dbx500-prcmu.h>
#include <mach/pm.h>

#define GPIO_DATA(_name, first, num)					\
	{								\
		.name		= _name,				\
		.first_gpio	= first,				\
		.first_irq	= NOMADIK_GPIO_TO_IRQ(first),		\
		.num_gpio	= num,					\
		.get_secondary_status = ux500_pm_gpio_read_wake_up_status, \
		.set_ioforce	= ux500_pm_prcmu_set_ioforce,		\
	}

#define GPIO_RESOURCE(block)						\
	{								\
		.start	= U5500_GPIOBANK##block##_BASE,			\
		.end	= U5500_GPIOBANK##block##_BASE + 127,		\
		.flags	= IORESOURCE_MEM,				\
	},								\
	{								\
		.start	= IRQ_DB5500_GPIO##block,			\
		.end	= IRQ_DB5500_GPIO##block,			\
		.flags	= IORESOURCE_IRQ,				\
	},								\
	{								\
		.start	= IRQ_DB5500_PRCMU_GPIO##block,			\
		.end	= IRQ_DB5500_PRCMU_GPIO##block,			\
		.flags	= IORESOURCE_IRQ,				\
	}

#define GPIO_DEVICE(block)						\
	{								\
		.name		= "gpio",				\
		.id		= block,				\
		.num_resources 	= 3,					\
		.resource	= &u5500_gpio_resources[block * 3],	\
		.dev = {						\
			.platform_data = &u5500_gpio_data[block],	\
		},							\
	}

static struct nmk_gpio_platform_data u5500_gpio_data[] = {
	GPIO_DATA("GPIO-0-31", 0, 32),
	GPIO_DATA("GPIO-32-63", 32, 4), /* 36..63 not routed to pin */
	GPIO_DATA("GPIO-64-95", 64, 19), /* 83..95 not routed to pin */
	GPIO_DATA("GPIO-96-127", 96, 6), /* 102..127 not routed to pin */
	GPIO_DATA("GPIO-128-159", 128, 21), /* 149..159 not routed to pin */
	GPIO_DATA("GPIO-160-191", 160, 32),
	GPIO_DATA("GPIO-192-223", 192, 32),
	GPIO_DATA("GPIO-224-255", 224, 4), /* 228..255 not routed to pin */
};

static struct resource u5500_gpio_resources[] = {
	GPIO_RESOURCE(0),
	GPIO_RESOURCE(1),
	GPIO_RESOURCE(2),
	GPIO_RESOURCE(3),
	GPIO_RESOURCE(4),
	GPIO_RESOURCE(5),
	GPIO_RESOURCE(6),
	GPIO_RESOURCE(7),
};

struct platform_device u5500_gpio_devs[] = {
	GPIO_DEVICE(0),
	GPIO_DEVICE(1),
	GPIO_DEVICE(2),
	GPIO_DEVICE(3),
	GPIO_DEVICE(4),
	GPIO_DEVICE(5),
	GPIO_DEVICE(6),
	GPIO_DEVICE(7),
};

#define U5500_PWM_SIZE 0x20
static struct resource u5500_pwm0_resource[] = {
	{
		.name = "PWM_BASE",
		.start = U5500_PWM_BASE,
		.end = U5500_PWM_BASE + U5500_PWM_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource u5500_pwm1_resource[] = {
	{
		.name = "PWM_BASE",
		.start = U5500_PWM_BASE + U5500_PWM_SIZE,
		.end = U5500_PWM_BASE + U5500_PWM_SIZE * 2 - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource u5500_pwm2_resource[] = {
	{
		.name = "PWM_BASE",
		.start = U5500_PWM_BASE + U5500_PWM_SIZE * 2,
		.end = U5500_PWM_BASE + U5500_PWM_SIZE * 3 - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource u5500_pwm3_resource[] = {
	{
		.name = "PWM_BASE",
		.start = U5500_PWM_BASE + U5500_PWM_SIZE * 3,
		.end = U5500_PWM_BASE + U5500_PWM_SIZE * 4 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device u5500_pwm0_device = {
	.id = 0,
	.name = "pwm",
	.resource = u5500_pwm0_resource,
	.num_resources = ARRAY_SIZE(u5500_pwm0_resource),
};

struct platform_device u5500_pwm1_device = {
	.id = 1,
	.name = "pwm",
	.resource = u5500_pwm1_resource,
	.num_resources = ARRAY_SIZE(u5500_pwm1_resource),
};

struct platform_device u5500_pwm2_device = {
	.id = 2,
	.name = "pwm",
	.resource = u5500_pwm2_resource,
	.num_resources = ARRAY_SIZE(u5500_pwm2_resource),
};

struct platform_device u5500_pwm3_device = {
	.id = 3,
	.name = "pwm",
	.resource = u5500_pwm3_resource,
	.num_resources = ARRAY_SIZE(u5500_pwm3_resource),
};

#ifdef CONFIG_FB_MCDE
static struct resource mcde_resources[] = {
	[0] = {
		.name  = MCDE_IO_AREA,
		.start = U5500_MCDE_BASE,
		.end   = U5500_MCDE_BASE + U5500_MCDE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name  = MCDE_IO_AREA,
		.start = U5500_DSI_LINK1_BASE,
		.end   = U5500_DSI_LINK1_BASE + U5500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.name  = MCDE_IO_AREA,
		.start = U5500_DSI_LINK2_BASE,
		.end   = U5500_DSI_LINK2_BASE + U5500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.name  = MCDE_IRQ,
		.start = IRQ_DB5500_DISP,
		.end   = IRQ_DB5500_DISP,
		.flags = IORESOURCE_IRQ,
	},
};

static int mcde_platform_enable_dsipll(void)
{
	return prcmu_enable_dsipll();
}

static int mcde_platform_disable_dsipll(void)
{
	return prcmu_disable_dsipll();
}

static int mcde_platform_set_display_clocks(void)
{
	return prcmu_set_display_clocks();
}

static struct mcde_platform_data mcde_pdata = {
	.num_dsilinks = 2,
	.syncmux = 0x01,
	.num_channels = 2,
	.num_overlays = 3,
	.regulator_mcde_epod_id = "vsupply",
	.regulator_esram_epod_id = "v-esram12",
#ifdef CONFIG_MCDE_DISPLAY_DSI
	.clock_dsi_id = "hdmi",
	.clock_dsi_lp_id = "tv",
#endif
	.clock_mcde_id = "mcde",
	.platform_set_clocks = mcde_platform_set_display_clocks,
	.platform_enable_dsipll = mcde_platform_enable_dsipll,
	.platform_disable_dsipll = mcde_platform_disable_dsipll,
};

struct platform_device u5500_mcde_device = {
	.name = "mcde",
	.id = -1,
	.dev = {
		.platform_data = &mcde_pdata,
	},
	.num_resources = ARRAY_SIZE(mcde_resources),
	.resource = mcde_resources,
};
#endif

static struct resource b2r2_resources[] = {
	[0] = {
		.start	= U5500_B2R2_BASE,
		.end	= U5500_B2R2_BASE + ((4*1024)-1),
		.name	= "b2r2_base",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name  = "B2R2_IRQ",
		.start = IRQ_DB5500_B2R2,
		.end   = IRQ_DB5500_B2R2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device u5500_b2r2_device = {
	.name	= "b2r2",
	.id	= 0,
	.dev	= {
		.init_name = "b2r2_bus",
		.coherent_dma_mask = ~0,
	},
	.num_resources	= ARRAY_SIZE(b2r2_resources),
	.resource	= b2r2_resources,
};

static struct resource u5500_thsens_resources[] = {
	[0] = {
		.name	= "IRQ_HOTMON_LOW",
		.start  = IRQ_DB5500_PRCMU_TEMP_SENSOR_LOW,
		.end    = IRQ_DB5500_PRCMU_TEMP_SENSOR_LOW,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.name	= "IRQ_HOTMON_HIGH",
		.start  = IRQ_DB5500_PRCMU_TEMP_SENSOR_HIGH,
		.end    = IRQ_DB5500_PRCMU_TEMP_SENSOR_HIGH,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device u5500_thsens_device = {
	.name           = "db5500_temp",
	.resource       = u5500_thsens_resources,
	.num_resources  = ARRAY_SIZE(u5500_thsens_resources),
};
