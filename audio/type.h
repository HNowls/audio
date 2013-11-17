/*
 * type.h
 *
 *  Created on: 2013-11-13
 *      Author: hemiao
 */


#ifndef TYPE_H
#define TYPE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#define AVCODEC_MAX_AUDIO_FRAME_SIZE (1024 * 1024)

typedef struct video_status
{
	AVFormatContext		* format_ctx_ptr;
	AVCodecContext 		* audio_codec_ctx_ptr;
	AVCodec				* audio_codec_ptr;
	int 				  audio_stream_index;

	AVPacket			  audio_packet;
	AVFrame				* frame;
	uint8_t 			* audio_buf;
	DECLARE_ALIGNED(16,uint8_t,audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
	uint8_t				  nb_samples;
	int					  src_sample_rate, dst_sample_rate;
	int					  audio_buf_index;
	int 				  audio_buf_size;

	packet_queue_t		  audio_queue;

	struct SwrContext	* swr_ctx;
	enum AVSampleFormat   audio_src_fmt;
	enum AVSampleFormat   audio_tgt_fmt;
	int             	  audio_src_channels;
	int             	  audio_tgt_channels;
	int64_t         	  audio_src_channel_layout;
	int64_t         	  audio_tgt_channel_layout;
	int             	  audio_src_freq;
	int             	  audio_tgt_freq;

	SDL_Event			  event;
}video_status_t;

#endif
