/*
 * zoom2.c  --  SoC audio for Zoom2
 *
 * Author: Misael Lopez Cruz <x0052729@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/board-zoom.h>
#include <plat/mcbsp.h>

/* Register descriptions for twl4030 codec part */
#include <linux/mfd/twl4030-audio.h>
#include <linux/module.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"

#define ZOOM2_HEADSET_MUX_GPIO		(OMAP_MAX_GPIO_LINES + 15)

static int zoom2_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 26000000,
					SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops zoom2_ops = {
	.hw_params = zoom2_hw_params,
};

/* Zoom2 machine DAPM */
static const struct snd_soc_dapm_widget zoom2_twl4030_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Ext Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_LINE("Aux In", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* External Mics: MAINMIC, SUBMIC with bias*/
	{"MAINMIC", NULL, "Mic Bias 1"},
	{"SUBMIC", NULL, "Mic Bias 2"},
	{"Mic Bias 1", NULL, "Ext Mic"},
	{"Mic Bias 2", NULL, "Ext Mic"},

	/* External Speakers: HFL, HFR */
	{"Ext Spk", NULL, "HFL"},
	{"Ext Spk", NULL, "HFR"},

	/* Headset Stereophone:  HSOL, HSOR */
	{"Headset Stereophone", NULL, "HSOL"},
	{"Headset Stereophone", NULL, "HSOR"},

	/* Headset Mic: HSMIC with bias */
	{"HSMIC", NULL, "Headset Mic Bias"},
	{"Headset Mic Bias", NULL, "Headset Mic"},

	/* Aux In: AUXL, AUXR */
	{"Aux In", NULL, "AUXL"},
	{"Aux In", NULL, "AUXR"},
};

static int zoom2_twl4030_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* TWL4030 not connected pins */
	snd_soc_dapm_nc_pin(dapm, "CARKITMIC");
	snd_soc_dapm_nc_pin(dapm, "DIGIMIC0");
	snd_soc_dapm_nc_pin(dapm, "DIGIMIC1");
	snd_soc_dapm_nc_pin(dapm, "EARPIECE");
	snd_soc_dapm_nc_pin(dapm, "PREDRIVEL");
	snd_soc_dapm_nc_pin(dapm, "PREDRIVER");
	snd_soc_dapm_nc_pin(dapm, "CARKITL");
	snd_soc_dapm_nc_pin(dapm, "CARKITR");

	return 0;
}

static int zoom2_twl4030_voice_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	unsigned short reg;

	/* Enable voice interface */
	reg = codec->driver->read(codec, TWL4030_REG_VOICE_IF);
	reg |= TWL4030_VIF_DIN_EN | TWL4030_VIF_DOUT_EN | TWL4030_VIF_EN;
	codec->driver->write(codec, TWL4030_REG_VOICE_IF, reg);

	return 0;
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link zoom2_dai[] = {
	{
		.name = "TWL4030 I2S",
		.stream_name = "TWL4030 Audio",
		.cpu_dai_name = "omap-mcbsp-dai.1",
		.codec_dai_name = "twl4030-hifi",
		.platform_name = "omap-pcm-audio",
		.codec_name = "twl4030-codec",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
		.init = zoom2_twl4030_init,
		.ops = &zoom2_ops,
	},
	{
		.name = "TWL4030 PCM",
		.stream_name = "TWL4030 Voice",
		.cpu_dai_name = "omap-mcbsp-dai.2",
		.codec_dai_name = "twl4030-voice",
		.platform_name = "omap-pcm-audio",
		.codec_name = "twl4030-codec",
		.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_IB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
		.init = zoom2_twl4030_voice_init,
		.ops = &zoom2_ops,
	},
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_zoom2 = {
	.name = "Zoom2",
	.dai_link = zoom2_dai,
	.num_links = ARRAY_SIZE(zoom2_dai),

	.dapm_widgets = zoom2_twl4030_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(zoom2_twl4030_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static int __devinit zoom2_soc_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_zoom2;
	int ret;

	pr_info("Zoom2 SoC init\n");

	card->dev = &pdev->dev;

	BUG_ON(gpio_request(ZOOM2_HEADSET_MUX_GPIO, "hs_mux") < 0);
	gpio_direction_output(ZOOM2_HEADSET_MUX_GPIO, 0);

	BUG_ON(gpio_request(ZOOM2_HEADSET_EXTMUTE_GPIO, "ext_mute") < 0);
	gpio_direction_output(ZOOM2_HEADSET_EXTMUTE_GPIO, 0);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
			ret);
		goto err;
	}

	return 0;

err:
	gpio_free(ZOOM2_HEADSET_MUX_GPIO);
	gpio_free(ZOOM2_HEADSET_EXTMUTE_GPIO);

	return ret;
}

static int __devexit zoom2_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	gpio_free(ZOOM2_HEADSET_MUX_GPIO);
	gpio_free(ZOOM2_HEADSET_EXTMUTE_GPIO);

	return 0;
}

static struct platform_driver zoom2_driver = {
	.driver = {
		.name = "zoom2-soc-audio",
		.owner = THIS_MODULE,
	},

	.probe = zoom2_soc_probe,
	.remove = __devexit_p(zoom2_soc_remove),
};

static int __init zoom2_soc_init(void)
{
	return platform_driver_register(&zoom2_driver);
}
module_init(zoom2_soc_init);

static void __exit zoom2_soc_exit(void)
{
	platform_driver_unregister(&zoom2_driver);
}
module_exit(zoom2_soc_exit);

MODULE_AUTHOR("Misael Lopez Cruz <x0052729@ti.com>");
MODULE_DESCRIPTION("ALSA SoC Zoom2");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:zoom2-soc-audio");
