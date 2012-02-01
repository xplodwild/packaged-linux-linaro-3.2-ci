/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Hanumath Prasad <hanumath.prasad@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/amba/bus.h>
#include <linux/amba/mmci.h>
#include <linux/mmc/host.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <plat/ste_dma40.h>
#include <mach/devices.h>
#include <mach/hardware.h>
#include <mach/ste-dma40-db8500.h>

#include "devices-db8500.h"
#include "board-mop500.h"

#undef CONFIG_STE_DMA40 /* Temporarily disable DMA */

/*
 * SDI 0 (MicroSD slot)
 */

/* GPIO pins used by the sdi0 level shifter */
static int sdi0_en = -1;
static int sdi0_vsel = -1;

static int mop500_sdi0_ios_handler(struct device *dev, struct mmc_ios *ios)
{
	switch (ios->power_mode) {
	case MMC_POWER_UP:
	case MMC_POWER_ON:
		/*
		 * Level shifter voltage should depend on vdd to when deciding
		 * on either 1.8V or 2.9V. Once the decision has been made the
		 * level shifter must be disabled and re-enabled with a changed
		 * select signal in order to switch the voltage. Since there is
		 * no framework support yet for indicating 1.8V in vdd, use the
		 * default 2.9V.
		 */
		gpio_direction_output(sdi0_vsel, 0);
		gpio_direction_output(sdi0_en, 1);
		udelay(100);
		break;
	case MMC_POWER_OFF:
		gpio_direction_output(sdi0_vsel, 0);
		gpio_direction_output(sdi0_en, 0);
		break;
	}

	return 0;
}

#ifdef CONFIG_STE_DMA40
struct stedma40_chan_cfg mop500_sdi0_dma_cfg_rx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type = DB8500_DMA_DEV1_SD_MMC0_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
	.use_fixed_channel = true,
	.phy_channel = 0,
};

static struct stedma40_chan_cfg mop500_sdi0_dma_cfg_tx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB8500_DMA_DEV1_SD_MMC0_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
	.use_fixed_channel = true,
	.phy_channel = 0,
};
#endif

static struct mmci_platform_data mop500_sdi0_data = {
	.ios_handler	= mop500_sdi0_ios_handler,
	.ocr_mask	= MMC_VDD_29_30,
	.f_max		= 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_SD_HIGHSPEED |
				MMC_CAP_MMC_HIGHSPEED,
	.gpio_wp	= -1,
	.sigdir		= MCI_ST_FBCLKEN |
				MCI_ST_CMDDIREN |
				MCI_ST_DATA0DIREN |
				MCI_ST_DATA2DIREN,
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &mop500_sdi0_dma_cfg_rx,
	.dma_tx_param	= &mop500_sdi0_dma_cfg_tx,
#endif
};

/*
 * SDI1 (SDIO WLAN)
 */
#ifdef SDIO_DMA_ON
#ifdef CONFIG_STE_DMA40
static struct stedma40_chan_cfg sdi1_dma_cfg_rx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type = DB8500_DMA_DEV32_SD_MM1_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};

static struct stedma40_chan_cfg sdi1_dma_cfg_tx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB8500_DMA_DEV32_SD_MM1_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};
#endif
#endif

static struct mmci_platform_data mop500_sdi1_data = {
	.ocr_mask	= MMC_VDD_29_30,
	.f_max		= 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA,
	.gpio_cd	= -1,
	.gpio_wp	= -1,
#ifdef SDIO_DMA_ON
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &sdi1_dma_cfg_rx,
	.dma_tx_param	= &sdi1_dma_cfg_tx,
#endif
#endif
};

static void sdi0_sdi1_configure(void)
{
        int ret;
	u32 periphid = 0;


        ret = gpio_request(sdi0_en, "level shifter enable");
        if (!ret)
                ret = gpio_request(sdi0_vsel,
                                   "level shifter 1v8-3v select");

        if (ret) {
                pr_warning("unable to config sdi0 gpios for level shifter.\n");
                return;
        }

        /* Select the default 2.9V and enable level shifter */
        gpio_direction_output(sdi0_vsel, 0);
        gpio_direction_output(sdi0_en, 1);

	/* v2 has a new version of this block that need to be forced */
	if (cpu_is_u8500v20_or_later())
		periphid = 0x10480180;

	db8500_add_sdi0(&mop500_sdi0_data, periphid);
	db8500_add_sdi1(&mop500_sdi1_data, periphid);
}

void mop500_sdi_tc35892_init(void)
{
	mop500_sdi0_data.gpio_cd = GPIO_SDMMC_CD;
	sdi0_en = GPIO_SDMMC_EN;
	sdi0_vsel = GPIO_SDMMC_1V8_3V_SEL;
	sdi0_sdi1_configure();
}

/*
 * SDI 2 (POP eMMC, not on DB8500ed)
 */
#ifdef CONFIG_STE_DMA40
struct stedma40_chan_cfg mop500_sdi2_dma_cfg_rx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type =  DB8500_DMA_DEV28_SD_MM2_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};

static struct stedma40_chan_cfg mop500_sdi2_dma_cfg_tx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB8500_DMA_DEV28_SD_MM2_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};
#endif

static struct mmci_platform_data mop500_sdi2_data = {
	.ocr_mask	= MMC_VDD_165_195,
	.f_max		= 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
	.gpio_cd	= -1,
	.gpio_wp	= -1,
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &mop500_sdi2_dma_cfg_rx,
	.dma_tx_param	= &mop500_sdi2_dma_cfg_tx,
#endif
};

/*
 * SDI 4 (on-board eMMC)
 */

#ifdef CONFIG_STE_DMA40
struct stedma40_chan_cfg mop500_sdi4_dma_cfg_rx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type =  DB8500_DMA_DEV42_SD_MM4_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};

static struct stedma40_chan_cfg mop500_sdi4_dma_cfg_tx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB8500_DMA_DEV42_SD_MM4_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};
#endif

static struct mmci_platform_data mop500_sdi4_data = {
	.ocr_mask	= MMC_VDD_29_30,
	.f_max		= 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
			  MMC_CAP_MMC_HIGHSPEED,
	.gpio_cd	= -1,
	.gpio_wp	= -1,
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &mop500_sdi4_dma_cfg_rx,
	.dma_tx_param	= &mop500_sdi4_dma_cfg_tx,
#endif
};

void __init mop500_sdi_init(void)
{
	u32 periphid = 0;

	/* v2 has a new version of this block that need to be forced */
	if (cpu_is_u8500v20_or_later()) {
		periphid = 0x10480180;

		/* PoP:ed eMMC on DB8500 v1.0 has problems with high speed */
		mop500_sdi2_data.capabilities |= MMC_CAP_MMC_HIGHSPEED;
	}

	db8500_add_sdi2(&mop500_sdi2_data, periphid);

	/* On-board eMMC */
	db8500_add_sdi4(&mop500_sdi4_data, periphid);

	/*
	 * On boards with the TC35892 GPIO expander, sdi0 and sdi1 will finally
	 * be added when the TC35892 initializes and calls
	 * mop500_sdi_tc35892_init() above.
	 */
}

void __init snowball_sdi_init(void)
{
	u32 periphid = 0x10480180;

	/* On Snowball MMC_CAP_SD_HIGHSPEED isn't supported (Hardware issue?) */
	mop500_sdi0_data.capabilities &= ~MMC_CAP_SD_HIGHSPEED;

	/* On-board eMMC */
	db8500_add_sdi4(&mop500_sdi4_data, periphid);

	mop500_sdi0_data.gpio_cd = SNOWBALL_SDMMC_CD_GPIO;
	mop500_sdi0_data.cd_invert = true;
	sdi0_en = SNOWBALL_SDMMC_EN_GPIO;
	sdi0_vsel = SNOWBALL_SDMMC_1V8_3V_GPIO;
	sdi0_sdi1_configure();
}

void __init hrefv60_sdi_init(void)
{
	u32 periphid = 0x10480180;

	mop500_sdi2_data.capabilities |= MMC_CAP_MMC_HIGHSPEED;

	db8500_add_sdi2(&mop500_sdi2_data, periphid);

	/* On-board eMMC */
	db8500_add_sdi4(&mop500_sdi4_data, periphid);

	mop500_sdi0_data.gpio_cd = HREFV60_SDMMC_CD_GPIO;
	sdi0_en = HREFV60_SDMMC_EN_GPIO;
	sdi0_vsel = HREFV60_SDMMC_1V8_3V_GPIO;
	sdi0_sdi1_configure();
}
