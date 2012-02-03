/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL), version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mfd/stmpe.h>
#include <linux/input/bu21013.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#ifdef CONFIG_U8500_FLASH
#include <../drivers/staging/camera_flash/adp1653_plat.h>
#endif
#include <linux/input/matrix_keypad.h>
#include <asm/mach-types.h>

#include "board-mop500.h"

/*
 * ux500 keymaps
 *
 * Organized row-wise as on the UIB, starting at the top-left
 *
 * we support two key layouts, specific to requirements. The first
 * keylayout includes controls for power/volume a few generic keys;
 * the second key layout contains the full numeric layout, enter/back/left
 * buttons along with a "."(dot), specifically for connectivity testing
 */
static const unsigned int mop500_keymap[] = {
#if defined(CONFIG_KEYLAYOUT_LAYOUT1)
	KEY(2, 5, KEY_END),
	KEY(4, 1, KEY_HOME),
	KEY(3, 5, KEY_VOLUMEDOWN),
	KEY(1, 3, KEY_EMAIL),
	KEY(5, 2, KEY_RIGHT),
	KEY(5, 0, KEY_BACKSPACE),

	KEY(0, 5, KEY_MENU),
	KEY(7, 6, KEY_ENTER),
	KEY(4, 5, KEY_0),
	KEY(6, 7, KEY_DOT),
	KEY(3, 4, KEY_UP),
	KEY(3, 3, KEY_DOWN),

	KEY(6, 4, KEY_SEND),
	KEY(6, 2, KEY_BACK),
	KEY(4, 2, KEY_VOLUMEUP),
	KEY(5, 5, KEY_SPACE),
	KEY(4, 3, KEY_LEFT),
	KEY(3, 2, KEY_SEARCH),
#elif defined(CONFIG_KEYLAYOUT_LAYOUT2)
	KEY(2, 5, KEY_RIGHT),
	KEY(4, 1, KEY_ENTER),
	KEY(3, 5, KEY_MENU),
	KEY(1, 3, KEY_3),
	KEY(5, 2, KEY_6),
	KEY(5, 0, KEY_9),

	KEY(0, 5, KEY_UP),
	KEY(7, 6, KEY_DOWN),
	KEY(4, 5, KEY_0),
	KEY(6, 7, KEY_2),
	KEY(3, 4, KEY_5),
	KEY(3, 3, KEY_8),

	KEY(6, 4, KEY_LEFT),
	KEY(6, 2, KEY_BACK),
	KEY(4, 2, KEY_KPDOT),
	KEY(5, 5, KEY_1),
	KEY(4, 3, KEY_4),
	KEY(3, 2, KEY_7),
#else
#warning "No keypad layout defined."
#endif
};

static const struct matrix_keymap_data mop500_keymap_data = {
	.keymap		= mop500_keymap,
	.keymap_size    = ARRAY_SIZE(mop500_keymap),
};
/*
 * STMPE1601
 */
static struct stmpe_keypad_platform_data stmpe1601_keypad_data = {
	.debounce_ms    = 64,
	.scan_count     = 8,
	.no_autorepeat  = true,
	.keymap_data    = &mop500_keymap_data,
};

static struct stmpe_platform_data stmpe1601_data = {
	.id		= 1,
	.blocks		= STMPE_BLOCK_KEYPAD,
	.irq_trigger    = IRQF_TRIGGER_FALLING,
	.irq_base       = MOP500_STMPE1601_IRQ(0),
	.keypad		= &stmpe1601_keypad_data,
	.autosleep      = true,
	.autosleep_timeout = 1024,
};

static struct i2c_board_info __initdata mop500_i2c0_devices_stuib[] = {
	{
		I2C_BOARD_INFO("stmpe1601", 0x40),
		.irq = NOMADIK_GPIO_TO_IRQ(218),
		.platform_data = &stmpe1601_data,
		.flags = I2C_CLIENT_WAKE,
	},
};

#ifdef CONFIG_U8500_FLASH
/*
 *  Config data for the flash
 */
static struct adp1653_platform_data __initdata adp1653_pdata_u8500_uib = {
	.irq_no = CAMERA_FLASH_INT_PIN
};
#endif

static struct i2c_board_info __initdata mop500_i2c2_devices_stuib[] = {
#ifdef CONFIG_U8500_FLASH
	{
		I2C_BOARD_INFO("adp1653", 0x30),
		.platform_data = &adp1653_pdata_u8500_uib
	}
#endif
};

/*
 * BU21013 ROHM touchscreen interface on the STUIBs
 */

/* tracks number of bu21013 devices being enabled */
static int bu21013_devices;

#define TOUCH_GPIO_PIN  84

#define TOUCH_XMAX	384
#define TOUCH_YMAX	704

#define PRCMU_CLOCK_OCR		0x1CC
#define TSC_EXT_CLOCK_9_6MHZ	0x840000

/**
 * bu21013_gpio_board_init : configures the touch panel.
 * @reset_pin: reset pin number
 * This function can be used to configures
 * the voltage and reset the touch panel controller.
 */
static int bu21013_gpio_board_init(int reset_pin)
{
	int retval = 0;

	bu21013_devices++;
	if (bu21013_devices == 1) {
		retval = gpio_request(reset_pin, "touchp_reset");
		if (retval) {
			printk(KERN_ERR "Unable to request gpio reset_pin");
			return retval;
		}
		retval = gpio_direction_output(reset_pin, 1);
		if (retval < 0) {
			printk(KERN_ERR "%s: gpio direction failed\n",
					__func__);
			return retval;
		}
		gpio_set_value_cansleep(reset_pin, 1);
	}

	return retval;
}

/**
 * bu21013_gpio_board_exit : deconfigures the touch panel controller
 * @reset_pin: reset pin number
 * This function can be used to deconfigures the chip selection
 * for touch panel controller.
 */
static int bu21013_gpio_board_exit(int reset_pin)
{
	int retval = 0;

	if (bu21013_devices == 1) {
		retval = gpio_direction_output(reset_pin, 0);
		if (retval < 0) {
			printk(KERN_ERR "%s: gpio direction failed\n",
					__func__);
			return retval;
		}
		gpio_set_value_cansleep(reset_pin, 0);
		gpio_free(reset_pin);
	}
	bu21013_devices--;

	return retval;
}

/**
 * bu21013_read_pin_val : get the interrupt pin value
 * This function can be used to get the interrupt pin value for touch panel
 * controller.
 */
static int bu21013_read_pin_val(void)
{
	return gpio_get_value(TOUCH_GPIO_PIN);
}

static struct bu21013_platform_device tsc_plat_device = {
	.cs_en = bu21013_gpio_board_init,
	.cs_dis = bu21013_gpio_board_exit,
	.irq_read_val = bu21013_read_pin_val,
	.irq = NOMADIK_GPIO_TO_IRQ(TOUCH_GPIO_PIN),
	.touch_x_max = TOUCH_XMAX,
	.touch_y_max = TOUCH_YMAX,
	.x_max_res = 480,
	.y_max_res = 864,
	.portrait = true,
	.has_ext_clk = true,
	.enable_ext_clk = false,
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE) &&	\
		CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
};

static struct bu21013_platform_device tsc_plat2_device = {
	.cs_en = bu21013_gpio_board_init,
	.cs_dis = bu21013_gpio_board_exit,
	.irq_read_val = bu21013_read_pin_val,
	.irq = NOMADIK_GPIO_TO_IRQ(TOUCH_GPIO_PIN),
	.touch_x_max = TOUCH_XMAX,
	.touch_y_max = TOUCH_YMAX,
	.x_max_res = 480,
	.y_max_res = 864,
	.portrait = true,
	.has_ext_clk = true,
	.enable_ext_clk = false,
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE) &&	\
		CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
};

static struct i2c_board_info __initdata u8500_i2c3_devices_stuib[] = {
	{
		I2C_BOARD_INFO("bu21013_ts", 0x5C),
		.platform_data = &tsc_plat_device,
	},
	{
		I2C_BOARD_INFO("bu21013_ts", 0x5D),
		.platform_data = &tsc_plat2_device,
	},

};

void __init mop500_stuib_init(void)
{
	if (machine_is_hrefv60()) {
		tsc_plat_device.cs_pin = HREFV60_TOUCH_RST_GPIO;
		tsc_plat2_device.cs_pin = HREFV60_TOUCH_RST_GPIO;
#ifdef CONFIG_U8500_FLASH
		adp1653_pdata_u8500_uib.enable_gpio =
					HREFV60_CAMERA_FLASH_ENABLE;
#endif
	} else {
		tsc_plat_device.cs_pin = GPIO_BU21013_CS;
		tsc_plat2_device.cs_pin = GPIO_BU21013_CS;
#ifdef CONFIG_U8500_FLASH
		adp1653_pdata_u8500_uib.enable_gpio =
					GPIO_CAMERA_FLASH_ENABLE;
#endif
	}

	mop500_uib_i2c_add(0, mop500_i2c0_devices_stuib,
			ARRAY_SIZE(mop500_i2c0_devices_stuib));

	mop500_uib_i2c_add(2, mop500_i2c2_devices_stuib,
			ARRAY_SIZE(mop500_i2c2_devices_stuib));

	mop500_uib_i2c_add(3, u8500_i2c3_devices_stuib,
			ARRAY_SIZE(u8500_i2c3_devices_stuib));
}
