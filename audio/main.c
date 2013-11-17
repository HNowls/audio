/*
 * main.c
 *
 *  Created on: 2013-11-13
 *      Author: hemiao
 */

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include "packet_queue.h"

#include "type.h"

int quit = 0;
#define SDL_AUDIO_BUFFER_SIZE 1024

int audio_decode_packet(video_status_t * vi, uint8_t * dst)
{
	int 		  ret;
	AVPacket 	* packet = &vi->audio_packet;
	int 		  got_frame = 0;
	static int 	  audio_data_size = 0;
	int 		  decoded_data_size;
	int64_t 	  decode_channel_layout;
	int 		  len2;

	static int count = 0;


	while (1)
	{
		while (audio_data_size > 0)
		{
			if (!vi->frame)
			{
				if (!(vi->frame = avcodec_alloc_frame()) < 0)
				{
					return AVERROR(ENOMEM);
				}
			} else {
				avcodec_get_frame_defaults(vi->frame);
			}
			ret = avcodec_decode_audio4(vi->audio_codec_ctx_ptr, vi->frame, &got_frame, packet);

			if (ret < 0)
			{
				audio_data_size = 0;
				break;
			}

			if (!got_frame)
				continue;

			audio_data_size -= ret;
			decoded_data_size = av_samples_get_buffer_size(NULL,
														   vi->frame->channels,
														   vi->frame->nb_samples,
														   vi->frame->format,
														   1);

			decode_channel_layout = (vi->frame->channel_layout && vi->frame->channels ==
									 av_get_channel_layout_nb_channels(vi->frame->channel_layout) ?
									 vi->frame->channel_layout : av_get_default_channel_layout(vi->frame->channels));

			if (count < 1)
			{
				count++;
				printf("audio tgt\n\tchannels : %d\n", vi->audio_tgt_channels);
				printf("\tformat: %d\n", vi->audio_tgt_fmt);
				printf("\tsample rate: %d\n", vi->audio_tgt_freq);
				printf("\tchannel layout: %d\n", vi->audio_tgt_channel_layout);
				printf("\n");
				printf("current frame\n\tchannels: %d\n", vi->frame->channels);
				printf("\tformat: %d\n", vi->frame->format);
				printf("\tsample rate: %d\n", vi->frame->sample_rate);
				printf("\tchannel layout: %d\n", vi->frame->channel_layout);
			}

			if (vi->audio_tgt_channels != vi->frame->channels ||
				vi->audio_tgt_fmt != vi->frame->format ||
				vi->audio_tgt_freq!= vi->frame->sample_rate ||
				vi->audio_tgt_channel_layout != decode_channel_layout)
			{
				if (vi->swr_ctx)layout:
					swr_free(&vi->swr_ctx);

				vi->swr_ctx = swr_alloc_set_opts(NULL,
												 vi->audio_tgt_channel_layout,
												 vi->audio_tgt_fmt,
												 vi->audio_tgt_freq,
												 vi->frame->channel_layout,
												 vi->frame->format,
												 vi->frame->sample_rate,
												 0, NULL);
				if (!vi->swr_ctx || swr_init(vi->swr_ctx) < 0)
				{
					fprintf(stderr, "swr_init failed.\n");
					break;
				}
			}

			if (vi->swr_ctx)
			{
				const uint8_t **in = (const uint8_t **)vi->frame->extended_data;
				uint8_t * out[] = {vi->audio_buf2};

//				fprintf(stderr, "spec.channels: %d\n", vi->spec.channels);
//				fprintf(stderr, "format: %d\n", vi->frame->format);
//				fprintf(stderr, "bytes per samples: %d\n", av_get_bytes_per_sample(vi->frame->format));
//				fprintf(stderr, "frame->nb_samples: %d\n", vi->frame->nb_samples);
//				fprintf(stderr, "out count: %d\n", sizeof(vi->audio_buf2) / vi->spec.channels / av_get_bytes_per_sample(vi->frame->format));
				len2 = swr_convert(vi->swr_ctx,
								   out,
								   sizeof(vi->audio_buf2)
								   / vi->audio_tgt_channels
								   / av_get_bytes_per_sample(vi->audio_tgt_fmt),
								   in,
								   vi->frame->nb_samples);
				if (len2 < 0)
				{
					fprintf(stderr, "swr_convert failed.\n");
					break;
				}

				vi->audio_buf = vi->audio_buf2;
				//decoded_data_size = len2 * vi->audio_tgt_channels * av_get_bytes_per_sample(vi->audio_tgt_fmt);
				decoded_data_size = av_samples_get_buffer_size(NULL,
															   vi->audio_tgt_channels,
															   len2,
															   vi->audio_tgt_fmt,
															   1);
			}

			return decoded_data_size;
		}

		if (packet->data)
				av_free_packet(packet);
			memset(packet, 0, sizeof(*packet));

		if (packet_queue_get(&vi->audio_queue, packet, 1) < 0)
			return -1;

		audio_data_size = packet->size;
	}

	return ret;
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{
	video_status_t * vi = (video_status_t *)userdata;
	int	ret, remain, size;

	while (len > 0)
	{
		if (vi->audio_buf_index >= vi->audio_buf_size)
		{
			ret = audio_decode_packet(vi, vi->audio_buf);

			if (ret > 0)
				vi->audio_buf_size = ret;
			if (ret == 0)
				vi->audio_buf_size = 1024;

			vi->audio_buf_index = 0;
		}

		remain = vi->audio_buf_size - vi->audio_buf_index;
		size = len;
		if (len > remain)
			size = remain;

		memcpy(stream, vi->audio_buf + vi->audio_buf_index, size);
		stream += size;
		vi->audio_buf_index += size;
		len -= size;
	}
}

int ui_init(video_status_t * vi)
{
	SDL_AudioSpec wanted_spec, spec;
	int64_t wanted_channel_layout = 0;
	int64_t wanted_nb_channels = 0;

	SDL_Init(SDL_INIT_AUDIO);

	wanted_nb_channels = vi->audio_codec_ctx_ptr->channels;
	if (wanted_channel_layout != av_get_default_channel_layout(wanted_nb_channels))
	{
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}
	wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = vi;
	wanted_spec.silence = 0;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.freq = vi->audio_codec_ctx_ptr->sample_rate;

	if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
		fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());

	fprintf(stderr, "%d: wanted_spec.format = %d\n", __LINE__, wanted_spec.format);
	fprintf(stderr, "%d: wanted_spec.samples = %d\n", __LINE__, wanted_spec.samples);
	fprintf(stderr, "%d: wanted_spec.channels = %d\n", __LINE__, wanted_spec.channels);
	fprintf(stderr, "%d: wanted_spec.freq = %d\n", __LINE__, wanted_spec.freq);
	fprintf(stderr, "%d: spec.format = %d\n", __LINE__, spec.format);
	fprintf(stderr, "%d: spec.samples = %d\n", __LINE__, spec.samples);
	fprintf(stderr, "%d: spec.channels = %d\n", __LINE__, spec.channels);
	fprintf(stderr, "%d: spec.freq = %d\n", __LINE__, spec.freq);

	wanted_channel_layout = av_get_default_channel_layout(spec.channels);
	vi->audio_src_channel_layout = vi->audio_tgt_channel_layout = wanted_channel_layout;
	vi->audio_src_channels = vi->audio_tgt_channels = spec.channels;
	vi->audio_src_fmt = vi->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
	vi->audio_src_freq = vi->audio_tgt_freq = spec.freq;

	SDL_PauseAudio(0);

	return 0;
}

int main(int argc, char ** argv)
{
	video_status_t * vi;
	AVPacket packet;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s input_file", argv[0]);
		exit(-1);
	}

	av_register_all();
	vi = av_malloc(sizeof(*vi));
	memset(vi, 0, sizeof(*vi));

	vi->format_ctx_ptr = avformat_alloc_context();
	if (avformat_open_input(&vi->format_ctx_ptr, argv[1], NULL, NULL))
	{
		fprintf(stderr, "%s error.\n", __FUNCTION__);
		goto end;
	}

	avformat_find_stream_info(vi->format_ctx_ptr, NULL);
	av_dump_format(vi->format_ctx_ptr, 0, argv[1], 0);

	vi->audio_stream_index = av_find_best_stream(vi->format_ctx_ptr,
			AVMEDIA_TYPE_AUDIO, -1, 0, &vi->audio_codec_ptr, 0);
	if (vi->audio_stream_index < 0)
	{
		av_log(vi->format_ctx_ptr, AV_LOG_ERROR, "can't find audio stream");
	}
	vi->audio_codec_ctx_ptr = vi->format_ctx_ptr->streams[vi->audio_stream_index]->codec;
	vi->audio_codec_ptr = avcodec_find_decoder(vi->audio_codec_ctx_ptr->codec_id);

	packet_queue_init(&vi->audio_queue);
	ui_init(vi);

	if (avcodec_open2(vi->audio_codec_ctx_ptr, vi->audio_codec_ptr, NULL))
	{
		av_log(vi->format_ctx_ptr, AV_LOG_ERROR, "can't open audio stream");
	}

	while(1)
	{
		if (vi->audio_queue.nb_packets > 20)
		{
			SDL_Delay(10);
			continue;
		}
		if (av_read_frame(vi->format_ctx_ptr, &packet))
		{
			fprintf(stderr, "av_read_frame failed.\n");
			continue;
		}

		if (packet.stream_index == vi->audio_stream_index)
		{
			packet_queue_put(&vi->audio_queue, &packet);
		}

		SDL_PollEvent(&vi->event);
		switch (vi->event.type)
		{
		case SDL_QUIT:
			quit = 1;
			SDL_Quit();
			break;
		default:
			break;
		}
	}

end:
	avformat_free_context(vi->format_ctx_ptr);
	av_frame_free(&vi->frame);
	av_free(vi);
	exit(0);
}
