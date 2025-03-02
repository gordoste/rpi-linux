/*
 * ASoC Driver for ezsound6x8 isolated soundcard
 *
 *  Created on: 01-Mar-2025
 *      Author: gordoste@iinet.net.au
 *              based on other sound/soc/bcm drivers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/types.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "../codecs/pcm3168a.h"

static int snd_ezsound6x8_startup(struct snd_pcm_substream *substream)
{
	int ret = snd_pcm_hw_constraint_minmax(substream->runtime, SNDRV_PCM_HW_PARAM_RATE,
		96000, 96000);
	return ret;
}

static int snd_ezsound6x8_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai;
	int i, ret;

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		switch (params_rate(params)){
			case 96000:
				ret = snd_soc_dai_set_sysclk(codec_dai, 0, 24576000, SND_SOC_CLOCK_IN);
				if (ret && ret != -ENOTSUPP)
					return ret;
				break;
			default:
				return -EINVAL;
		}

		ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 2, 32);
		if (ret && ret != -ENOTSUPP)
			return ret;
	}
	return 0;
}

/* machine stream operations */
static struct snd_soc_ops snd_ezsound6x8_ops = {
	.startup = snd_ezsound6x8_startup,
	.hw_params = snd_ezsound6x8_hw_params,
};

static int ezsound6x8_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai;
	int i, ret;

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, 24576000, SND_SOC_CLOCK_IN);
		if (ret && ret != -ENOTSUPP)
			return ret;

		ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 2, 32);
		if (ret && ret != -ENOTSUPP)
			return ret;
	}
	return 0;
}

SND_SOC_DAILINK_DEFS(ezsound6x8_dac,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2835-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("pcm3168a.1-0045", "pcm3168a-dac")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2835-i2s.0")));

SND_SOC_DAILINK_DEFS(ezsound6x8_adc,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2835-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("pcm3168a.1-0045", "pcm3168a-adc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2835-i2s.0")));

static struct snd_soc_dai_link ezsound6x8_dai[] = {
	{
		.name = "ezsound6x8-dac",
		.stream_name = "ez6x8-playback",
		.ops = &snd_ezsound6x8_ops,
		.init = ezsound6x8_dai_init,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_I2S \
			| SND_SOC_DAIFMT_NB_NF,
		SND_SOC_DAILINK_REG(ezsound6x8_dac),
	},
	{
		.name = "ezsound6x8-adc",
		.stream_name = "ez6x8-capture",
		.ops = &snd_ezsound6x8_ops,
		.init = ezsound6x8_dai_init,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_I2S \
			| SND_SOC_DAIFMT_NB_NF,
		SND_SOC_DAILINK_REG(ezsound6x8_adc),
	},
};

static struct snd_soc_card snd_soc_ezsound6x8 = {
	.name = "ezsound6x8",
	.owner = THIS_MODULE,
	.dai_link = ezsound6x8_dai,
	.num_links = ARRAY_SIZE(ezsound6x8_dai),
};

static int ezsound6x8_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_ezsound6x8;
	int ret;

	card->dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct snd_soc_dai_link *dai;
		struct device_node *i2s_node = of_parse_phandle(pdev->dev.of_node,
								"i2s-controller", 0);

		if (i2s_node) {
			for (int i = 0; i < snd_soc_ezsound6x8.num_links; i++) {
				dai = &(ezsound6x8_dai[i]);
				dai->cpus->dai_name = NULL;
				dai->cpus->of_node = i2s_node;
				dai->platforms->name = NULL;
				dai->platforms->of_node = i2s_node;
			}
			of_node_put(i2s_node);
		} else
			if (!dai->cpus->of_node) {
				dev_err(&pdev->dev, "Property 'i2s-controller' missing or invalid\n");
				return -EINVAL;
			}
	}

	if ((ret = devm_snd_soc_register_card(&pdev->dev, card)))
		return dev_err_probe(&pdev->dev, ret, "%s\n", __func__);

	dev_info(&pdev->dev, "successfully loaded\n");

	return ret;
}

static const struct of_device_id ezsound6x8_of_match[] = {
	{ .compatible = "ezsound,6x8iso", },
	{},
};
MODULE_DEVICE_TABLE(of, ezsound6x8_of_match);

static struct platform_driver ezsound6x8_driver = {
       .driver         = {
		.name   = "ezsound6x8-driver",
		.owner  = THIS_MODULE,
		.of_match_table = ezsound6x8_of_match,
       },
       .probe          = ezsound6x8_probe,
};

module_platform_driver(ezsound6x8_driver);
MODULE_AUTHOR("Stephen Gordon <gordoste@iinet.net.au>");
MODULE_DESCRIPTION("ezsound 6x8 soundcard");
MODULE_LICENSE("GPL v2");
