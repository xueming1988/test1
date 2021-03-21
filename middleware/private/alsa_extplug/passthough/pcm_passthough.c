/*
 * Automatic passthrough plugin
 *
 * Copyright (c) 2006 by Takashi Iwai <tiwai@suse.de>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

typedef struct snd_pcm_passthrough snd_pcm_passthrough_t;

struct snd_pcm_passthrough {
	snd_pcm_extplug_t ext;
	/* setup */
	/* privates */
};

static snd_pcm_sframes_t
passthrough_transfer(snd_pcm_extplug_t *ext,
	       const snd_pcm_channel_area_t *dst_areas,
	       snd_pcm_uframes_t dst_offset,
	       const snd_pcm_channel_area_t *src_areas,
	       snd_pcm_uframes_t src_offset,
	       snd_pcm_uframes_t size)
{
	snd_pcm_passthrough_t *passthrough = (snd_pcm_passthrough_t *)ext;

	snd_pcm_area_copy(dst_areas, dst_offset, src_areas, src_offset, size, SND_PCM_FORMAT_S32);

	return size;
}

static int passthrough_init(snd_pcm_extplug_t *ext)
{
	snd_pcm_passthrough_t *passthrough = (snd_pcm_passthrough_t *)ext;
	printf("%s\n", __func__);
	return 0;
}

static int passthrough_close(snd_pcm_extplug_t *ext)
{
	snd_pcm_passthrough_t *passthrough = (snd_pcm_passthrough_t *)ext;
	printf("%s\n", __func__);
	return 0;
}

static const snd_pcm_extplug_callback_t passthrough_callback = {
	.transfer = passthrough_transfer,
	.init = passthrough_init,
	.close = passthrough_close,
};

SND_PCM_PLUGIN_DEFINE_FUNC(linkplay_passthough)
{
	snd_config_iterator_t i, next;
	snd_pcm_passthrough_t *passthrough;
	snd_config_t *sconf = NULL;
	unsigned int channels = 0;
	int frame = 0;
	char *str;
	int err;
	printf("%s\n", __func__);

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 || strcmp(id, "hint") == 0)
			continue;
		if (strcmp(id, "slave") == 0) {
			sconf = n;
			continue;
		}
		if (strcmp(id, "frames") == 0) {
			long val;
			err = snd_config_get_integer(n, &val);
			if (err < 0) {
				SNDERR("Invalid value for %s", id);
				return err;
			}
			frame = val;
			printf("frame get:%d\n", frame);
			continue;
		}
		if (strcmp(id, "configPath") == 0) {
			long val;
			err = snd_config_get_string(n, &str);
			if (err < 0) {
				SNDERR("Invalid value for %s", id);
				return err;
			}
			printf("configPath get:%s\n", str);
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}

	if (! sconf) {
		SNDERR("No slave configuration for filter linkplay_passthough pcm");
		return -EINVAL;
	}

	passthrough = calloc(1, sizeof(*passthrough));
	if (passthrough == NULL)
		return -ENOMEM;

	passthrough->ext.version = SND_PCM_EXTPLUG_VERSION;
	passthrough->ext.name = "linkplay_passthough Plugin";
	passthrough->ext.callback = &passthrough_callback;
	passthrough->ext.private_data = passthrough;

	err = snd_pcm_extplug_create(&passthrough->ext, name, root, sconf, stream, mode);
	if (err < 0) {
		free(passthrough);
		return err;
	}

	snd_pcm_extplug_set_param(&passthrough->ext,
                                SND_PCM_EXTPLUG_HW_CHANNELS,
                                2);
	snd_pcm_extplug_set_slave_param(&passthrough->ext,
                                SND_PCM_EXTPLUG_HW_CHANNELS,
                                2);
	snd_pcm_extplug_set_param(&passthrough->ext, SND_PCM_EXTPLUG_HW_FORMAT,
                                SND_PCM_FORMAT_S32);
	snd_pcm_extplug_set_slave_param(&passthrough->ext, SND_PCM_EXTPLUG_HW_FORMAT,
                                SND_PCM_FORMAT_S32);

	*pcmp = passthrough->ext.pcm;
	return 0;
}

SND_PCM_PLUGIN_SYMBOL(linkplay_passthough);
