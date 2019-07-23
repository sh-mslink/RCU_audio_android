/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 × new version for ADPCM decode
 */

#define LOG_TAG "audio_hw_sklrm"
/*#define LOG_NDEBUG 0*/
#define DUALHID 1

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <linux/hid.h>

#include <sys/time.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include <tinyalsa/asoundlib.h>

#include <audio_utils/resampler.h>

#include <cutils/properties.h>

#include <audio_hw_sklrm.h>

#define PCM_CARD 2
#define PCM_DEVICE 0
#define PCM_DEVICE_SCO 2

#define MIXER_CARD 1

#define OUT_PERIOD_SIZE 512
#define OUT_SHORT_PERIOD_COUNT 2
#define OUT_LONG_PERIOD_COUNT 8
#define OUT_SAMPLING_RATE 44100

#define IN_PERIOD_SIZE 475*2
#define IN_PERIOD_COUNT 2
#define IN_SAMPLING_RATE 16000

#define SCO_PERIOD_SIZE 256
#define SCO_PERIOD_COUNT 4
#define SCO_SAMPLING_RATE 8000

#define BLOCKLENGTH 68
#define DBGRAWDATA 0

#define BT_REMOTE_VERSION_41 0 
#define BT_REMOTE_VERSION_42 1

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 2000
#define MAX_WRITE_SLEEP_US ((OUT_PERIOD_SIZE * OUT_SHORT_PERIOD_COUNT * 1000000) \
                                / OUT_SAMPLING_RATE)

enum {
	OUT_BUFFER_TYPE_UNKNOWN, OUT_BUFFER_TYPE_SHORT, OUT_BUFFER_TYPE_LONG,
};

struct pcm_config pcm_config_out = { .channels = 2, .rate = OUT_SAMPLING_RATE,
		.period_size = OUT_PERIOD_SIZE, .period_count = OUT_LONG_PERIOD_COUNT,
		.format = PCM_FORMAT_S16_LE, .start_threshold = OUT_PERIOD_SIZE
				* OUT_SHORT_PERIOD_COUNT, };

struct pcm_config pcm_config_in = { .channels = 1, .rate = IN_SAMPLING_RATE,
		.period_size = IN_PERIOD_SIZE, .period_count = IN_PERIOD_COUNT,
		.format = PCM_FORMAT_S16_LE, .start_threshold = 1, .stop_threshold =
				(IN_PERIOD_SIZE * IN_PERIOD_COUNT), };

struct pcm_config pcm_config_sco = { .channels = 1, .rate = SCO_SAMPLING_RATE,
		.period_size = SCO_PERIOD_SIZE, .period_count = SCO_PERIOD_COUNT,
		.format = PCM_FORMAT_S16_LE, };

struct audio_device {
	struct audio_hw_device hw_device;

	pthread_mutex_t lock; /* see note below on mutex acquisition order */
	unsigned int out_device;
	unsigned int in_device;
	bool standby;
	bool mic_mute;
	struct audio_route *ar;
	int orientation;
	bool screen_off;

	struct stream_out *active_out;
	struct stream_in *active_in;


	

};

struct stream_out {
	struct audio_stream_out stream;

	pthread_mutex_t lock; /* see note below on mutex acquisition order */
	struct pcm *pcm;
	struct pcm_config *pcm_config;
	bool standby;
	uint64_t written; /* total frames written, not cleared when entering standby */

	struct resampler_itfe *resampler;
	int16_t *buffer;
	size_t buffer_frames;

	int write_threshold;
	int cur_write_threshold;
	int buffer_type;

	struct audio_device *dev;
};

struct stream_in {
	struct audio_stream_in stream;

	pthread_mutex_t lock; /* see note below on mutex acquisition order */
	struct pcm *pcm;
	struct pcm_config *pcm_config;
	bool standby;

	unsigned int requested_rate;
	struct resampler_itfe *resampler;
	struct resampler_buffer_provider buf_provider;
	int16_t *buffer;
	size_t buffer_size;
	size_t frames_in;
	int read_status;

	struct audio_device *dev;
	int fd;


};

enum {
	ORIENTATION_LANDSCAPE,
	ORIENTATION_PORTRAIT,
	ORIENTATION_SQUARE,
	ORIENTATION_UNDEFINED,
};

static uint32_t out_get_sample_rate(const struct audio_stream *stream);
static size_t out_get_buffer_size(const struct audio_stream *stream);
static audio_format_t out_get_format(const struct audio_stream *stream);
static uint32_t in_get_sample_rate(const struct audio_stream *stream);
static size_t in_get_buffer_size(const struct audio_stream *stream);
static audio_format_t in_get_format(const struct audio_stream *stream);
static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
		struct resampler_buffer* buffer);
static void release_buffer(struct resampler_buffer_provider *buffer_provider,
		struct resampler_buffer* buffer);

static int hidrawnum;
static int fd_hidraw;

static int scanHIDRAWNUM(void);

/*
 * NOTE: when multiple mutexes have to be acquired, always take the
 * audio_device mutex first, followed by the stream_in and/or
 * stream_out mutexes.E/audio_hw_sklrm(  582): read data:-1 - f9 b3 b3 0

 */


int add(unsigned char *inbuff, unsigned char *outbuff,
		int len_of_in/*, struct adpcm_state *state*/);

 

/* must be called with hw device and output stream mutexes locked */
static void do_out_standby(struct stream_out *out) {
	struct audio_device *adev = out->dev;

	if (!out->standby) {
		pcm_close(out->pcm);
		out->pcm = NULL;
		adev->active_out = NULL;
		if (out->resampler) {
			release_resampler(out->resampler);
			out->resampler = NULL;
		}
		if (out->buffer) {
			free(out->buffer);
			out->buffer = NULL;
		}
		out->standby = true;
	}
}

/* must be called with hw device and input stream mutexes locked */
static void do_in_standby(struct stream_in *in) {
	struct audio_device *adev = in->dev;

	ALOGE(" do in standby");
	if (!in->standby) {
		//pcm_close(in->pcm);
 		close(fd_hidraw);

//        close(in->fd);
		in->pcm = NULL;

		adev->active_in = NULL;
		if (in->resampler) {
			release_resampler(in->resampler);
			in->resampler = NULL;
		}
		if (in->buffer) {
			free(in->buffer);
			in->buffer = NULL;
		}
		in->standby = true;
	}
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out) {
	struct audio_device *adev = out->dev;
	unsigned int device;
	int ret;
	int hidraw_path;
	/*
	 * Due to the lack of sample rate converters in the SoC,
	 * it greatly simplifies things to have only the main
	 * (speaker/headphone) PCM or the BC SCO PCM open at
	 * the same time.
	 */
	if (adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
		device = PCM_DEVICE_SCO;
		out->pcm_config = &pcm_config_sco;
	} else {
		device = PCM_DEVICE;
		out->pcm_config = &pcm_config_out;
		out->buffer_type = OUT_BUFFER_TYPE_UNKNOWN;
	}

	/*
	 * All open PCMs can only use a single group of rates at once:
	 * Group 1: 11.025, 22.05, 44.1
	 * Group 2: 8, 16, 32, 48
	 * Group 1 is used for digital audio playback since 44.1 is
	 * the most common rate, but group 2 is required for SCO.
	 */
	if (adev->active_in) {
		struct stream_in *in = adev->active_in;
		pthread_mutex_lock(&in->lock);
		if (((out->pcm_config->rate % 8000 == 0)
				&& (in->pcm_config->rate % 8000) != 0)
				|| ((out->pcm_config->rate % 11025 == 0)
						&& (in->pcm_config->rate % 11025) != 0))
			do_in_standby(in);
		pthread_mutex_unlock(&in->lock);
	}

//	out->pcm = pcm_open(PCM_CARD, device,
//			PCM_OUT | PCM_NORESTART | PCM_MONOTONIC, out->pcm_config);

	if (out->pcm && !pcm_is_ready(out->pcm)) {
		ALOGE("spcm_open(out) failed: %s", pcm_get_error(out->pcm));
		pcm_close(out->pcm);
		return -ENOMEM;
	}

	/*
	 * If the stream rate differs from the PCM rate, we need to
	 * create a resampler.
	 */
	if (out_get_sample_rate(&out->stream.common) != out->pcm_config->rate) {
		ret = create_resampler(out_get_sample_rate(&out->stream.common),
				out->pcm_config->rate, out->pcm_config->channels,
				RESAMPLER_QUALITY_DEFAULT, NULL, &out->resampler);
		out->buffer_frames =
				(pcm_config_out.period_size * out->pcm_config->rate)
						/ out_get_sample_rate(&out->stream.common) + 1;

		out->buffer = malloc(pcm_frames_to_bytes(out->pcm, out->buffer_frames));
	}

	adev->active_out = out;

	return 0;
}

static int scan_snd_num();

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct stream_in *in) {
	struct audio_device *adev = in->dev;
	unsigned int device;
	int ret;
	int pcm_num;
	char hidraw_path[20];

	/*
	 * Due to the lack of sample rate converters in the SoC,
	 * it greatly simplifies things to have only the main
	 * mic PCM or the BC SCO PCM open at the same time.
	 */
	if (adev->in_device & AUDIO_DEVICE_IN_ALL_SCO) {
		device = PCM_DEVICE_SCO;
		in->pcm_config = &pcm_config_sco;
	} else {
		device = PCM_DEVICE;
		in->pcm_config = &pcm_config_in;
	}

	/*
	 * All open PCMs can only use a single group of rates at once:
	 * Group 1: 11.025, 22.05, 44.1
	 * Group 2: 8, 16, 32, 48
	 * Group 1 is used for digital audio playback since 44.1 is
	 * the most common rate, but group 2 is required for SCO.
	 */
	if (adev->active_out) {
		struct stream_out *out = adev->active_out;
		pthread_mutex_lock(&out->lock);
		if (((in->pcm_config->rate % 8000 == 0)
				&& (out->pcm_config->rate % 8000) != 0)
				|| ((in->pcm_config->rate % 11025 == 0)
						&& (out->pcm_config->rate % 11025) != 0))
			do_out_standby(out);
		pthread_mutex_unlock(&out->lock);
	}

	pcm_num = scan_snd_num();
	in->pcm = pcm_open(pcm_num, device, PCM_IN, in->pcm_config);

	if (in->pcm && !pcm_is_ready(in->pcm)) {
		ALOGE("pcm_open(in) failed: %s", pcm_get_error(in->pcm));
		pcm_close(in->pcm);
		return -ENOMEM;
	}

	/*
	 * If the stream rate differs from the PCM rate, we need to
	 * create a resampler.
	 */
	if (in_get_sample_rate(&in->stream.common) != in->pcm_config->rate) {
		in->buf_provider.get_next_buffer = get_next_buffer;
		in->buf_provider.release_buffer = release_buffer;

		ret = create_resampler(in->pcm_config->rate,
				in_get_sample_rate(&in->stream.common), 1,
				RESAMPLER_QUALITY_DEFAULT, &in->buf_provider, &in->resampler);
	}
	in->buffer_size = pcm_frames_to_bytes(in->pcm, in->pcm_config->period_size);
	in->buffer = malloc(in->buffer_size);
	in->frames_in = 0;

	adev->active_in = in;

	return 0;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
		struct resampler_buffer* buffer) {
	struct stream_in *in;
	ALOGE("get_next_buffer");

	if (buffer_provider == NULL || buffer == NULL)
		return -EINVAL;

	in = (struct stream_in *)((char *)buffer_provider -
			offsetof(struct stream_in, buf_provider));

	if (in->pcm == NULL) {
		buffer->raw = NULL;
		buffer->frame_count = 0;
		in->read_status = -ENODEV;
		return -ENODEV;
	}

	if (in->frames_in == 0) {
		in->read_status = pcm_read(in->pcm, (void*) in->buffer,
				in->buffer_size);

		if (in->read_status != 0) {
			ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
			buffer->raw = NULL;
			buffer->frame_count = 0;
			return in->read_status;
		}
		in->frames_in = in->pcm_config->period_size;
		if (in->pcm_config->channels == 2) {
			unsigned int i;

			/* Discard right channel */
			for (i = 1; i < in->frames_in; i++)
				in->buffer[i] = in->buffer[i * 2];
		}
	}

	buffer->frame_count =
			(buffer->frame_count > in->frames_in) ?
					in->frames_in : buffer->frame_count;
	buffer->i16 = in->buffer + (in->pcm_config->period_size - in->frames_in);

	return in->read_status;

}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
		struct resampler_buffer* buffer) {
	struct stream_in *in;

	if (buffer_provider == NULL || buffer == NULL)
		return;

	in = (struct stream_in *)((char *)buffer_provider -
			offsetof(struct stream_in, buf_provider));

	in->frames_in -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames) {
	ssize_t frames_wr = 0;

	while (frames_wr < frames) {
		size_t frames_rd = frames - frames_wr;
		if (in->resampler != NULL) {
			in->resampler->resample_from_provider(in->resampler,
					(int16_t *) ((char *) buffer
							+ frames_wr
									* audio_stream_frame_size(
											&in->stream.common)), &frames_rd);
		} else {
			struct resampler_buffer buf = { { raw : NULL, }, frame_count
					: frames_rd, };
			get_next_buffer(&in->buf_provider, &buf);
			if (buf.raw != NULL) {
				memcpy(
						(char *) buffer
								+ frames_wr
										* audio_stream_frame_size(
												&in->stream.common), buf.raw,
						buf.frame_count
								* audio_stream_frame_size(&in->stream.common));
				frames_rd = buf.frame_count;
			}
			release_buffer(&in->buf_provider, &buf);
		}
		/* in->read_status is updated by getNextBuffer() also called by
		 * in->resampler->resample_from_provider() */
		if (in->read_status != 0)
			return in->read_status;

		frames_wr += frames_rd;
	}
	return frames_wr;
}

/* API functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream) {
	return pcm_config_out.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
	return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream) {
	return pcm_config_out.period_size
			* audio_stream_frame_size((struct audio_stream *) stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream) {
	return AUDIO_CHANNEL_OUT_STEREO;
}

static audio_format_t out_get_format(const struct audio_stream *stream) {
	return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format) {
	return -ENOSYS;
}

static int out_standby(struct audio_stream *stream) {
	struct stream_out *out = (struct stream_out *) stream;

	pthread_mutex_lock(&out->dev->lock);
	pthread_mutex_lock(&out->lock);
	do_out_standby(out);
	pthread_mutex_unlock(&out->lock);
	pthread_mutex_unlock(&out->dev->lock);

	return 0;
}

static int out_dump(const struct audio_stream *stream, int fd) {
	return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs) {
	struct stream_out *out = (struct stream_out *) stream;
	struct audio_device *adev = out->dev;
	struct str_parms *parms;
	char value[32];
	int ret;
	unsigned int val;

	parms = str_parms_create_str(kvpairs);

	ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
			sizeof(value));
	pthread_mutex_lock(&adev->lock);
	if (ret >= 0) {
		val = atoi(value);
		if ((adev->out_device != val) && (val != 0)) {
			/*
			 * If SCO is turned on/off, we need to put audio into standby
			 * because SCO uses a different PCM.
			 */
			if ((val & AUDIO_DEVICE_OUT_ALL_SCO)
					^ (adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO)) {
				pthread_mutex_lock(&out->lock);
				do_out_standby(out);
				pthread_mutex_unlock(&out->lock);
			}

			adev->out_device = val;
			// select_devices(adev);
		}
	}
	pthread_mutex_unlock(&adev->lock);

	str_parms_destroy(parms);
	return ret;
}

static char * out_get_parameters(const struct audio_stream *stream,
		const char *keys) {
	return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream) {
	struct stream_out *out = (struct stream_out *) stream;
	struct audio_device *adev = out->dev;
	size_t period_count;

	pthread_mutex_lock(&adev->lock);

	if (adev->screen_off && !adev->active_in
			&& !(adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO))
		period_count = OUT_LONG_PERIOD_COUNT;
	else
		period_count = OUT_SHORT_PERIOD_COUNT;

	pthread_mutex_unlock(&adev->lock);

	return (pcm_config_out.period_size * period_count * 1000)
			/ pcm_config_out.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
		float right) {
	return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
		size_t bytes) {
	int ret = 0;
	struct stream_out *out = (struct stream_out *) stream;
	struct audio_device *adev = out->dev;
	size_t frame_size = audio_stream_frame_size(&out->stream.common);
	int16_t *in_buffer = (int16_t *) buffer;
	size_t in_frames = bytes / frame_size;
	size_t out_frames;
	int buffer_type;
	int kernel_frames;
	bool sco_on;

	/*
	 * acquiring hw device mutex systematically is useful if a low
	 * priority thread is waiting on the output stream mutex - e.g.
	 * executing out_set_parameters() while holding the hw device
	 * mutex
	 */
	pthread_mutex_lock(&adev->lock);
	pthread_mutex_lock(&out->lock);
	if (out->standby) {
		ret = start_output_stream(out);
		if (ret != 0) {
			pthread_mutex_unlock(&adev->lock);
			goto exit;
		}
		out->standby = false;
	}
	buffer_type =
			(adev->screen_off && !adev->active_in) ?
					OUT_BUFFER_TYPE_LONG : OUT_BUFFER_TYPE_SHORT;
	sco_on = (adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO);
	pthread_mutex_unlock(&adev->lock);

	/* detect changes in screen ON/OFF state and adapt buffer size
	 * if needed. Do not change buffer size when routed to SCO device. */
	if (!sco_on && (buffer_type != out->buffer_type)) {
		size_t period_count;

		if (buffer_type == OUT_BUFFER_TYPE_LONG)
			period_count = OUT_LONG_PERIOD_COUNT;
		else
			period_count = OUT_SHORT_PERIOD_COUNT;

		out->write_threshold = out->pcm_config->period_size * period_count;
		/* reset current threshold if exiting standby */
		if (out->buffer_type == OUT_BUFFER_TYPE_UNKNOWN)
			out->cur_write_threshold = out->write_threshold;
		out->buffer_type = buffer_type;
	}

	/* Reduce number of channels, if necessary */
	if (popcount(out_get_channels(&stream->common))
			> (int) out->pcm_config->channels) {
		unsigned int i;

		/* Discard right channel */
		for (i = 1; i < in_frames; i++)
			in_buffer[i] = in_buffer[i * 2];

		/* The frame size is now half */
		frame_size /= 2;
	}

	/* Change sample rate, if necessary */
	if (out_get_sample_rate(&stream->common) != out->pcm_config->rate) {
		out_frames = out->buffer_frames;
		out->resampler->resample_from_input(out->resampler, in_buffer,
				&in_frames, out->buffer, &out_frames);
		in_buffer = out->buffer;
	} else {
		out_frames = in_frames;
	}

	if (!sco_on) {
		int total_sleep_time_us = 0;
		size_t period_size = out->pcm_config->period_size;

		/* do not allow more than out->cur_write_threshold frames in kernel
		 * pcm driver buffer */
		do {
			struct timespec time_stamp;
			if (pcm_get_htimestamp(out->pcm, (unsigned int *) &kernel_frames,
					&time_stamp) < 0)
				break;
			kernel_frames = pcm_get_buffer_size(out->pcm) - kernel_frames;

			if (kernel_frames > out->cur_write_threshold) {
				int sleep_time_us = (int) (((int64_t)(
						kernel_frames - out->cur_write_threshold) * 1000000)
						/ out->pcm_config->rate);
				if (sleep_time_us < MIN_WRITE_SLEEP_US)
					break;
				total_sleep_time_us += sleep_time_us;
				if (total_sleep_time_us > MAX_WRITE_SLEEP_US) {
					ALOGW("out_write() limiting sleep time %d to %d",
							total_sleep_time_us, MAX_WRITE_SLEEP_US);
					sleep_time_us = MAX_WRITE_SLEEP_US
							- (total_sleep_time_us - sleep_time_us);
				}
				usleep(sleep_time_us);
			}

		} while ((kernel_frames > out->cur_write_threshold)
				&& (total_sleep_time_us <= MAX_WRITE_SLEEP_US));

		/* do not allow abrupt changes on buffer size. Increasing/decreasing
		 * the threshold by steps of 1/4th of the buffer size keeps the write
		 * time within a reasonable range during transitions.
		 * Also reset current threshold just above current filling status when
		 * kernel buffer is really depleted to allow for smooth catching up with
		 * target threshold.
		 */
		if (out->cur_write_threshold > out->write_threshold) {
			out->cur_write_threshold -= period_size / 4;
			if (out->cur_write_threshold < out->write_threshold) {
				out->cur_write_threshold = out->write_threshold;
			}
		} else if (out->cur_write_threshold < out->write_threshold) {
			out->cur_write_threshold += period_size / 4;
			if (out->cur_write_threshold > out->write_threshold) {
				out->cur_write_threshold = out->write_threshold;
			}
		} else if ((kernel_frames < out->write_threshold)
				&& ((out->write_threshold - kernel_frames)
						> (int) (period_size * OUT_SHORT_PERIOD_COUNT))) {
			out->cur_write_threshold = (kernel_frames / period_size + 1)
					* period_size;
			out->cur_write_threshold += period_size / 4;
		}
	}

	ret = pcm_write(out->pcm, in_buffer, out_frames * frame_size);
	if (ret == -EPIPE) {
		/* In case of underrun, don't sleep since we want to catch up asap */
		pthread_mutex_unlock(&out->lock);
		return ret;
	}
	if (ret == 0) {
		out->written += out_frames;
	}

	exit: pthread_mutex_unlock(&out->lock);

	if (ret != 0) {
		usleep(
				bytes * 1000000 / audio_stream_frame_size(&stream->common)
						/ out_get_sample_rate(&stream->common));
	}

	return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
		uint32_t *dsp_frames) {
	return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream,
		effect_handle_t effect) {
	return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream,
		effect_handle_t effect) {
	return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
		int64_t *timestamp) {
	return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
		uint64_t *frames, struct timespec *timestamp) {
	struct stream_out *out = (struct stream_out *) stream;
	int ret = -1;

	pthread_mutex_lock(&out->lock);

	size_t avail;
	if (pcm_get_htimestamp(out->pcm, &avail, timestamp) == 0) {
		size_t kernel_buffer_size = out->pcm_config->period_size
				* out->pcm_config->period_count;
		// FIXME This calculation is incorrect if there is buffering after app processor
		int64_t signed_frames = out->written - kernel_buffer_size + avail;
		// It would be unusual for this value to be negative, but check just in case ...
		if (signed_frames >= 0) {
			*frames = signed_frames;
			ret = 0;
		}
	}

	pthread_mutex_unlock(&out->lock);

	return ret;
}

// input channel
static int checkInfo(int num) {
	//Read vendor
	int res, desc_size = 0;
	char buf_dev[256];
	unsigned int k;

	struct hidraw_report_descriptor rpt_desc;
	struct hidraw_devinfo info;

	char hidraw_path[20];
	int fd = -1;
	int rd = 0;
	int val, i, ret;
	sprintf(hidraw_path, "/dev/hidraw%d", num);
	//sleep(1);

	fd = open(hidraw_path, O_RDONLY|O_NONBLOCK);
	if (fd < 0) {
		ALOGE("oepn hidraw%d fial ,errno = %d !\n", num, errno);
		return 0;
	}

	memset(&rpt_desc, 0x0, sizeof(rpt_desc));
	memset(&info, 0x0, sizeof(info));
	/* Get Report Descriptor Size */

	res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
	if (res < 0)
		goto check_err;

	/* Get Report Descriptor */
	rpt_desc.size = desc_size;
	res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
	if (res < 0)
		goto check_err;

	/* Get Raw Name */
	res = ioctl(fd, HIDIOCGRAWNAME(256), buf_dev);
	if (res < 0)
		goto check_err;

	/* Get Physical Location */

	res = ioctl(fd, HIDIOCGRAWPHYS(256), buf_dev);
	if (res < 0)
		goto check_err;

	/* Get Raw Info */

	res = ioctl(fd, HIDIOCGRAWINFO, &info);
	if (res < 0)
		goto check_err;

        if ((info.vendor == 0x04 || info.vendor == 0x200 || info.vendor == 0x00 || info.vendor ==0x00c4 || info.vendor == 0x1727) 
		&& (info.product == 0x00 || info.product == 0x7a44 || info.product == 0xb032)) {
		ALOGE("check hidraw%d success !\n", num);
		close(fd);
		return 1;
	} else {
		check_err: close(fd);
		ALOGE("check hidraw%d fail,errno = %d !\n", num,errno);
		return 0;
	}

}

//get hid number
static int scanHIDRAWNUM(void) {

	int i = 0;
	int fd;
	char tmpPath[15];

	for (i = 0; i < 64; i++) {
		if (checkInfo(i)) {
			hidrawnum = i;
			return 1;
		}
	}

	return 0;
}



/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream) {
	struct stream_in *in = (struct stream_in *) stream;

	return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
	return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream) {
	struct stream_in *in = (struct stream_in *) stream;
	size_t size;

	/*
	 * take resampling into account and return the closest majoring
	 * multiple of 16 frames, as audioflinger expects audio buffers to
	 * be a multiple of 16 frames
	 */
	size = (in->pcm_config->period_size * in_get_sample_rate(stream))
			/ in->pcm_config->rate;
	size = ((size + 15) / 16) * 16;

	//return size * audio_stream_frame_size((struct audio_stream *) stream);

	return IN_PERIOD_SIZE;
}

static uint32_t in_get_channels(const struct audio_stream *stream) {
	return AUDIO_CHANNEL_IN_MONO;
}

static audio_format_t in_get_format(const struct audio_stream *stream) {
	return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format) {
	return -ENOSYS;
}

static int in_standby(struct audio_stream *stream) {
	struct stream_in *in = (struct stream_in *) stream;

#if 1
	pthread_mutex_lock(&in->dev->lock);
	pthread_mutex_lock(&in->lock);
	do_in_standby(in);
	pthread_mutex_unlock(&in->lock);
	pthread_mutex_unlock(&in->dev->lock);
#endif
	return 0;
}

static int in_dump(const struct audio_stream *stream, int fd) {
	return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs) {
	struct stream_in *in = (struct stream_in *) stream;
	struct audio_device *adev = in->dev;
	struct str_parms *parms;
	char value[32];
	int ret;
	unsigned int val;

	parms = str_parms_create_str(kvpairs);

	ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
			sizeof(value));
	pthread_mutex_lock(&adev->lock);
	if (ret >= 0) {
		val = atoi(value) & ~AUDIO_DEVICE_BIT_IN;
		if ((adev->in_device != val) && (val != 0)) {
			/*
			 * If SCO is turned on/off, we need to put audio into standby
			 * because SCO uses a different PCM.
			 */
			if ((val & AUDIO_DEVICE_IN_ALL_SCO)
					^ (adev->in_device & AUDIO_DEVICE_IN_ALL_SCO)) {
				pthread_mutex_lock(&in->lock);
				do_in_standby(in);
				pthread_mutex_unlock(&in->lock);
			}

			adev->in_device = val;
		}
	}
	pthread_mutex_unlock(&adev->lock);

	str_parms_destroy(parms);
	return ret;
}

static char * in_get_parameters(const struct audio_stream *stream,
		const char *keys) {
	return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain) {
	return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
		size_t bytes) {
	int ret = 0;
	unsigned char buf[256];
	struct stream_in *in = (struct stream_in *) stream;
	struct audio_device *adev = in->dev;
	size_t frames_rq = bytes / audio_stream_frame_size(&stream->common);
	char hidraw_path[20];

	if (scanHIDRAWNUM()){
		//ALOGE(" find hidraw");
		sprintf(hidraw_path, "/dev/hidraw%d", hidrawnum);

		fd_hidraw = open(hidraw_path, O_RDWR|O_NONBLOCK);
		ALOGE(" open   /dev/hidraw%d OK!\n", hidrawnum);

		if (fd_hidraw < 0)
			ALOGE("fail to open   %s ,errno = %d!\n", hidraw_path, errno);
	}

	/*
	 * acquiring hw device mutex systematically is useful if a low
	 * priority thread is waiting on the input stream mutex - e.g.
	 * executing in_set_parameters() while holding the hw device
	 * mutex
	 */

	unsigned char data[IN_PERIOD_SIZE];
	unsigned char buf_dec[IN_PERIOD_SIZE];
	int rec_id;
	int preid = 0;
	int count = 0;
	int id = 0;
	int j = 0;

	ALOGE("read data start------------------------------------");

	while(1){
		memset(buf, 0, sizeof(buf));


#if 0
		ret = read(fd_hidraw, buf,21) ;

		#if DBGRAWDATA
			ALOGE("read data:%d - key no：%x , data:  %x %x %x", ret,buf[1],buf[2],buf[3],buf[4]);
		#endif 

		if (ret <= 0) {
			usleep(5000);
			count++;
			ALOGE("read again, errno:%d",errno);
		}

		if (count>10) {
			ALOGE("fail read data--------------------------------");
			close(fd_hidraw);
			memset(buffer, 0,512);			
			return bytes;
		}

       		if ((buf[1] == 0xfe) || (buf[1] == 0x0a)){   //test for taiji
            		rec_id= (int)buf[2]*256 + buf[3];
            		ALOGE("SKL---package rec_id=%d",rec_id);
			//goto finish;    
           		if(preid == rec_id){

                		ALOGE("same id");
				ALOGE("id count is %d",id);
				memcpy(data+17*id, buf+4, 17);
				#if DBGRAWDATA
				ALOGE("same id data - %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],buf[16],buf[17],buf[18],buf[19],buf[20]);
				#endif
				if (id > 2){
                			
					//do decoder
					add(data,buf_dec,BLOCKLENGTH);
					
					#if DBGRAWDATA
					for (j=0;j<31;j++){
						ALOGE("SKL decode - %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",buf_dec[0+j*16],buf_dec[1+j*16],buf_dec[2+j*16],buf_dec[3+j*16],buf_dec[4+j*16],buf_dec[5+j*16],buf_dec[6+j*16],buf_dec[7+j*16],buf_dec[8+j*16],buf_dec[9+j*16],buf_dec[10+j*16],buf_dec[11+j*16],buf_dec[12+j*16],buf_dec[13+j*16],buf_dec[14+j*16],buf_dec[15+j*16]);
						ALOGE("SKL decode ");   
					}
					#endif
					
					memcpy(buffer,buf_dec,512);
					memset(data, 0, sizeof(data));
					count = 0;
                			goto finish;
				}
				
				id++;
           		}
			else{

                		ALOGE("first id");
                		memcpy(data, buf+4, 17);
                		preid = rec_id;
				id = 1;
				#if DBGRAWDATA
				ALOGE("new id data - %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],buf[16],buf[17],buf[18],buf[19],buf[20]);
				#endif            			
			}
		}
 
	}

#endif		

#if 1
		ret = read(fd_hidraw, buf,245);
		
		//ALOGE("read dataa:%d - %x %x %x %x", ret,buf[1],buf[2],buf[3],buf[4]);
		//ALOGE("key no - %x",buf[1]);
		if (ret <= 0) {
			usleep(5000);
			count++;
			//ALOGE("read again");
		}

		if (count>10) {
	//		ALOGE("fail read data--------------------------------");
			close(fd_hidraw);
			memset(buffer, 0,IN_PERIOD_SIZE);			
			return bytes;
		}


       		if ((buf[1] == 0xfe) || (buf[1] == 0x0a)){   //test for taiji
       		//if (buf[1] == 0x0a) {

    			ALOGE("SKL voice "); 
ALOGE("data - %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",buf[0], buf[1], buf[2], buf[3], buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23],buf[24],buf[25],buf[26],buf[27],buf[28],buf[29],buf[30],buf[31]);  

            		rec_id= (int)buf[2]*256 + buf[3];
            		ALOGE("SKL---package rec_id=%d",rec_id);
			memcpy(data, buf+4, 241);
			add(data,buf_dec,241);
		//for (j=0;j<31;j++){
						//ALOGE("SKL decode - %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",buf_dec[0+j*16],buf_dec[1+j*16],buf_dec[2+j*16],buf_dec[3+j*16],buf_dec[4+j*16],buf_dec[5+j*16],buf_dec[6+j*16],buf_dec[7+j*16],buf_dec[8+j*16],buf_dec[9+j*16],buf_dec[10+j*16],buf_dec[11+j*16],buf_dec[12+j*16],buf_dec[13+j*16],buf_dec[14+j*16],buf_dec[15+j*16]);
						//ALOGE("SKL decode ");   
					//}
			ALOGE("SKL---decoded ok");
	    	    	memcpy(buffer,buf_dec,IN_PERIOD_SIZE);


	         	memset(data, 0, sizeof(data));
	    	    	count = 0;
               		goto finish;
	    	}

	}
#endif	

	if (ret > 0) {
		ret = 0;
	}

	/*
	 * Instead of writing zeroes here, we could trust the hardware
	 * to always provide zeroes when muted.
	 */
	if (ret == 0 && adev->mic_mute)
		memset(buffer, 0, bytes);

	exit: if (ret < 0) {
		usleep(
			bytes * 1000000 / audio_stream_frame_size(&stream->common)
			/ in_get_sample_rate(&stream->common));
		ALOGE("in read waiting");
	}

finish:
	pthread_mutex_unlock(&in->lock);
	ALOGE("input read finish:%d------------------------", bytes);
	close(fd_hidraw);

	ALOGE("close hid ");   

	return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream) {
	return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream,
		effect_handle_t effect) {
	return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream,
		effect_handle_t effect) {
	return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
		audio_io_handle_t handle, audio_devices_t devices,
		audio_output_flags_t flags, struct audio_config *config,
		struct audio_stream_out **stream_out) {
	struct audio_device *adev = (struct audio_device *) dev;
	struct stream_out *out;
	int ret;

	ALOGE("start open output stream");
	out = (struct stream_out *) calloc(1, sizeof(struct stream_out));
	if (!out)
		return -ENOMEM;

	out->stream.common.get_sample_rate = out_get_sample_rate;
	out->stream.common.set_sample_rate = out_set_sample_rate;
	out->stream.common.get_buffer_size = out_get_buffer_size;
	out->stream.common.get_channels = out_get_channels;
	out->stream.common.get_format = out_get_format;
	out->stream.common.set_format = out_set_format;
	out->stream.common.standby = out_standby;
	out->stream.common.dump = out_dump;
	out->stream.common.set_parameters = out_set_parameters;
	out->stream.common.get_parameters = out_get_parameters;
	out->stream.common.add_audio_effect = out_add_audio_effect;
	out->stream.common.remove_audio_effect = out_remove_audio_effect;
	out->stream.get_latency = out_get_latency;
	out->stream.set_volume = out_set_volume;
	out->stream.write = out_write;
	out->stream.get_render_position = out_get_render_position;
	out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
	out->stream.get_presentation_position = out_get_presentation_position;

	out->dev = adev;

	config->format = out_get_format(&out->stream.common);
	config->channel_mask = out_get_channels(&out->stream.common);
	config->sample_rate = out_get_sample_rate(&out->stream.common);

	out->standby = true;
	/* out->written = 0; by calloc() */

	*stream_out = &out->stream;

	ALOGE("finish open output stream");
	return 0;

	err_open: free(out);
	*stream_out = NULL;
	return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
		struct audio_stream_out *stream) {
	out_standby(&stream->common);
	free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs) {
	return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
		const char *keys) {
	return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev) {
	return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume) {
	return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume) {
	return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode) {
	return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state) {
	struct audio_device *adev = (struct audio_device *) dev;

	adev->mic_mute = state;

	return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state) {
	struct audio_device *adev = (struct audio_device *) dev;

	*state = adev->mic_mute;

	return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
		const struct audio_config *config) {
	size_t size;

	/*
	 * take resampling into account and return the closest majoring
	 * multiple of 16 frames, as audioflinger expects audio buffers to
	 * be a multiple of 16 frames
	 */
	size = (pcm_config_in.period_size * config->sample_rate)
			/ pcm_config_in.rate;
	size = ((size + 15) / 16) * 16;

	return (size * popcount(config->channel_mask)
			* audio_bytes_per_sample(config->format));
}

static int adev_open_input_stream(struct audio_hw_device *dev,
		audio_io_handle_t handle, audio_devices_t devices,
		struct audio_config *config, struct audio_stream_in **stream_in) {
	struct audio_device *adev = (struct audio_device *) dev;
	struct stream_in *in;
	int res = 0;
	char buf[2];
	char hidraw_path[20];

	*stream_in = NULL;

	ALOGE("start open input stream");
	#if 0
       //#if DUALHID
        /* Send a Report to the Device to open mic*/
        buf[0] = 0x03; /* Report Number */
        buf[1] = 0x01; /* Open mic */

        if (scanHIDRAWNUM()){

                sprintf(hidraw_path, "/dev/hidraw%d", hidrawnum);
                fd_hidraw = open(hidraw_path, O_RDWR|O_NONBLOCK);
                ALOGE(" open   /dev/hidraw%d OK!\n", hidrawnum);
                if (fd_hidraw < 0)
                        ALOGE("fail to open   %s ,errno = %d!\n", hidraw_path, errno);

        }

        res = write(fd_hidraw, buf, 2);
        if (res < 0) {
                ALOGV("Error write: %d\n", errno);
        } else {
                ALOGV("write() wrote %d bytes\n", res);
        }

        if (fd_hidraw>0)
                close(fd_hidraw);
        #endif




	/* Respond with a request for mono if a different format is given. */
	if (config->channel_mask != AUDIO_CHANNEL_IN_MONO) {
		config->channel_mask = AUDIO_CHANNEL_IN_MONO;
		return -EINVAL;
	}

	in = (struct stream_in *) calloc(1, sizeof(struct stream_in));
	if (!in)
		return -ENOMEM;

	in->stream.common.get_sample_rate = in_get_sample_rate;
	in->stream.common.set_sample_rate = in_set_sample_rate;
	in->stream.common.get_buffer_size = in_get_buffer_size;
	in->stream.common.get_channels = in_get_channels;
	in->stream.common.get_format = in_get_format;
	in->stream.common.set_format = in_set_format;
	in->stream.common.standby = in_standby;
	in->stream.common.dump = in_dump;
	in->stream.common.set_parameters = in_set_parameters;
	in->stream.common.get_parameters = in_get_parameters;
	in->stream.common.add_audio_effect = in_add_audio_effect;
	in->stream.common.remove_audio_effect = in_remove_audio_effect;
	in->stream.set_gain = in_set_gain;
	in->stream.read = in_read;
	in->stream.get_input_frames_lost = in_get_input_frames_lost;

	in->dev = adev;
	in->standby = true;
	in->requested_rate = config->sample_rate;
	in->pcm_config = &pcm_config_in; /* default PCM config */

	*stream_in = &in->stream;

	return 0;
}

static int adev_close_input_stream(struct audio_hw_device *dev,
		struct audio_stream_in *stream) {
	struct stream_in *in = (struct stream_in *) stream;
	struct audio_device *adev = (struct audio_device *) dev;
	in_standby(&stream->common);


	int res = 0;
        char hidraw_path[20];
	char buf[2];
	ALOGE("finish read");
	
	#if 1
	//#if DUALHID
        /* Send a Report to the Device to close mic*/
        buf[0] = 0x03; /* Report Number */
        buf[1] = 0x00; /* Close Mic */

        if (scanHIDRAWNUM()){
                //ALOGE(" find hidraw");
                sprintf(hidraw_path, "/dev/hidraw%d", hidrawnum);

                fd_hidraw = open(hidraw_path, O_RDWR);
                ALOGE(" open   /dev/hidraw%d OK!\n", hidrawnum);

                if (fd_hidraw < 0)
                        ALOGE("fail to open   %s ,errno = %d!\n", hidraw_path, errno);
        }

        res = write(fd_hidraw, buf, 2);
        if (res < 0) {
                ALOGV("Error write: %d\n", errno);
        } else {
               	ALOGV("write() wrote %d bytes\n", res);
        }

	#endif

	//关闭语音输入misc节点
	free(stream);
	return 0;

}

static int adev_dump(const audio_hw_device_t *device, int fd) {
	return 0;
}

static int adev_close(hw_device_t *device) {
	struct audio_device *adev = (struct audio_device *) device;
	free(device);
     	if (fd_hidraw > 0)
        	close(fd_hidraw);
        ALOGE("finish adev_close");
	return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
		hw_device_t** device) {
	struct audio_device *adev;

	int ret;

	ALOGE("start open aaudio device");

	if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
		return -EINVAL;

	adev = calloc(1, sizeof(struct audio_device));
	if (!adev)
		return -ENOMEM;

	adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
	adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
	adev->hw_device.common.module = (struct hw_module_t *) module;
	adev->hw_device.common.close = adev_close;

	adev->hw_device.init_check = adev_init_check;
	adev->hw_device.set_voice_volume = adev_set_voice_volume;
	adev->hw_device.set_master_volume = adev_set_master_volume;
	adev->hw_device.set_mode = adev_set_mode;
	adev->hw_device.set_mic_mute = adev_set_mic_mute;
	adev->hw_device.get_mic_mute = adev_get_mic_mute;
	adev->hw_device.set_parameters = adev_set_parameters;
	adev->hw_device.get_parameters = adev_get_parameters;
	adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
	adev->hw_device.open_output_stream = adev_open_output_stream;
	adev->hw_device.close_output_stream = adev_close_output_stream;
	adev->hw_device.open_input_stream = adev_open_input_stream;
	adev->hw_device.close_input_stream = adev_close_input_stream;
	adev->hw_device.dump = adev_dump;

	adev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
	adev->in_device = AUDIO_DEVICE_IN_BACK_MIC & ~AUDIO_DEVICE_BIT_IN;

	*device = &adev->hw_device.common;

	ALOGE("finish open a audio device");

	return 0;
}

static struct hw_module_methods_t hal_module_methods = { .open = adev_open, };

struct audio_module HAL_MODULE_INFO_SYM = { .common = { .tag =
		HARDWARE_MODULE_TAG, .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
		.hal_api_version = HARDWARE_HAL_API_VERSION, .id =
				AUDIO_HARDWARE_MODULE_ID, .name = "shockley audio HW HAL",
		.author = "The Android Open Source Project", .methods =
				&hal_module_methods, }, };
