/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include "../codecs/rt5660.h"

static const struct snd_soc_dapm_widget imx_rt5660_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
};

static const struct snd_soc_dapm_route imx_rt5660_dapm_routes[]={
	/* Mic Jack --> MIC_IN*/
	{"Mic Jack", NULL, "MICBIAS1"},
	{"IN1P", NULL, "Mic Jack"},
	{"IN1N", NULL, "Mic Jack"},

	/* LINE_OUT --> Ext Speaker */
	{"Line Out Jack", NULL, "LOUTL"},
	{"Line Out Jack", NULL, "LOUTR"},
};

static const struct snd_kcontrol_new imx_rt5660_controls[] = {
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
	SOC_DAPM_PIN_SWITCH("Line Out Jack"),
};

/*
 * Logic for a rt5660 as connected on a rockchip board.
 */
static int imx_rt5660_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_dapm_enable_pin(snd_soc_codec_get_dapm(codec), "Line Out Jack");
	snd_soc_dapm_enable_pin(snd_soc_codec_get_dapm(codec), "Mic Jack");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

	return 0;
}

static int imx_rt5660_hifi_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out = 0, dai_fmt = rtd->dai_link->dai_fmt;
	int ret;

	pr_debug("%s +\n", __func__);

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_fmt);
	if (ret < 0) {
		pr_err("%s - failed to set the format for codec side\n",
		       __func__);
		return ret;
	}

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_fmt);
	if (ret < 0) {
		pr_err("%s - failed to set the format for cpu side\n",
		       __func__);
		return ret;
	}

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
		pll_out = 12288000;
		break;
	case 11025:
	case 22050:
	case 44100:
		pll_out = 11289600;
		break;
	default:
		pr_err("%s: - Not support bit rate: %d\n", __func__,
		       params_rate(params));
		return -EINVAL;
	}

	pr_debug("%s - params_rate(params): %d\n", __func__,
	       params_rate(params));

	/*Set the system clk for codec */
	snd_soc_dai_set_pll(codec_dai, 0, RT5660_PLL1_S_MCLK, pll_out,
			    pll_out * 2);
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5660_SCLK_S_PLL1,
				     pll_out * 2, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		pr_err("%s: - Failed to set the sysclk for codec side\n",
			__func__);
		return ret;
	}

	snd_soc_dai_set_sysclk(cpu_dai, 0, pll_out, 0);
	snd_soc_dai_set_clkdiv(cpu_dai, 1,
			       (pll_out / 4) / params_rate(params) - 1);
	/* 256k = 48 - 1  3M = 3 */
	snd_soc_dai_set_clkdiv(cpu_dai, 0, 3);

	pr_debug("%s - LRCK: %d\n", __func__,
		 (pll_out / 4) / params_rate(params));
	pr_debug("%s -\n", __func__);


	return 0;
}

static struct snd_soc_ops imx_rt5660_hifi_ops = {
	.hw_params = imx_rt5660_hifi_hw_params,
};

enum {
	DAILINK_RT5660_HIFI,
};

#define DAILINK_ENTITIES	(DAILINK_RT5660_HIFI + 1)

static struct snd_soc_dai_link imx_rt5660_dailinks[] = {
	[DAILINK_RT5660_HIFI] = {
		.name = "RT5660 HIFI",
		.stream_name = "RT5660 PCM",
		.codec_dai_name = "rt5660-aif1",
		.ops = &imx_rt5660_hifi_ops,
		.init = &imx_rt5660_init,
		/* set rt5660 as slave */
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
	},
};

static struct snd_soc_card imx_rt5660_snd_card = {
	.name = "imx-rt5660",
	.dai_link = imx_rt5660_dailinks,
	.num_links = ARRAY_SIZE(imx_rt5660_dailinks),
	.controls = imx_rt5660_controls,
	.num_controls = ARRAY_SIZE(imx_rt5660_controls),
	.dapm_widgets    = imx_rt5660_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(imx_rt5660_dapm_widgets),
	.dapm_routes    = imx_rt5660_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(imx_rt5660_dapm_routes),
};

static int imx_rt5660_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &imx_rt5660_snd_card;
	int i, ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	for (i = 0; i < DAILINK_ENTITIES; i++) {
		imx_rt5660_dailinks[i].cpu_of_node =
			of_parse_phandle(pdev->dev.of_node, "audio-cpu", 0);
		if (!imx_rt5660_dailinks[i].cpu_of_node) {
			dev_err(&pdev->dev,	"Property(%d) 'audio-cpu' failed\n", i);
			return -EINVAL;
		}

		imx_rt5660_dailinks[i].codec_of_node =
			of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
		if (!imx_rt5660_dailinks[i].codec_of_node) {
			dev_err(&pdev->dev,	"Property[%d] 'audio-codec' failed\n", i);
			return -EINVAL;
		}
	}

	imx_rt5660_dailinks[0].platform_of_node = imx_rt5660_dailinks[0].cpu_of_node;
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err(&pdev->dev, "%s register card failed %d\n",
			__func__, ret);

	dev_info(&pdev->dev, "snd_soc_register_card successful\n");

	return ret;
}

static int imx_rt5660_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id imx_rt5660_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-rt5660", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_rt5660_dt_ids);

static struct platform_driver imx_rt5660_driver = {
	.driver = {
		.name = "imx-rt5660",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_rt5660_dt_ids,
	},
	.probe = imx_rt5660_probe,
	.remove = imx_rt5660_remove,
};
module_platform_driver(imx_rt5660_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX RT5660 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-rt5660");
