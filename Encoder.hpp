#pragma once
#include <iostream>
#include <string>
#include <functional>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
using namespace std;

#define IO_BUFFER_SIZE 4096
enum EncoderError {
SUCCESS,
FORMAT_CONTEXT_ERROR,
WRITE_HEADER_ERROR,
AVIO_OPEN_ERROR,
ALREADY_INIT_ERROR,
ALREADY_STOP_ERROR,
VIDEO_CODEC_ERROR,
VIDEO_CODEC_CONTEXT_ERROR,
VIDEO_SWS_GET_CONTEXT_ERROR,
VIDEO_NEW_STREAM_ERROR,
VIDEO_CODEC_OPEN_ERROR,
VIDEO_CODECPAR_ERROR,
VIDEO_ALLOC_FRAME_ERROR,
VIDEO_ALLOC_FRAME_BUFFER_ERROR,
VIDEO_ALLOC_PACKET_ERROR,
VIDEO_ENCODE_ERROR,
VIDEO_WRITE_FRAME_ERROR,
VIDEO_RECEIVE_PACKET_ERROR,
AUDIO_CODEC_ERROR,
AUDIO_CODEC_CONTEXT_ERROR,
AUDIO_SWR_ALLOC_ERROR,
AUDIO_SWR_INIT_ERROR,
AUDIO_NEW_STREAM_ERROR,
AUDIO_CODEC_OPEN_ERROR,
AUDIO_CODECPAR_ERROR,
AUDIO_ALLOC_FRAME_ERROR,
AUDIO_ALLOC_FRAME_BUFFER_ERROR,
AUDIO_ALLOC_PACKET_ERROR,
AUDIO_ENCODE_ERROR,
AUDIO_SWR_CONVERT_ERROR,
AUDIO_WRITE_FRAME_ERROR,
AUDIO_RECEIVE_PACKET_ERROR,
ENCODER_STOPPED
};

struct EncoderContext {
        std::string filePath;
        std::string videoCodecName;
        std::string audioCodecName;
        int width;
        int height;
        int framerate;
        int videoBitrate;
        int keyFramerate;
        AVPixelFormat inPixFmt;
       	AVSampleFormat inSmpFmt;
	AVPixelFormat outPixFmt;
	AVSampleFormat outSmpFmt;
        int audioBitrate;
        int samplerate;
        uint64_t channel_layout;
        int channels;
};

class Encoder {
public:
	Encoder();
	~Encoder();
	EncoderError initialize(EncoderContext eCtx, void* opaque, int (*encodedPacketCallback)(void*, uint8_t*, int));
	EncoderError stop();
	bool isStop();
	EncoderError encodeVideo(uint8_t* data, int size);
	EncoderError encodeAudio(uint8_t* data, int size);

private:
	struct EncoderContext eCtx;
	AVDictionary* dict;

	AVFormatContext *fmtCtx;
	struct SwsContext *swsCtx;
	struct SwrContext *swrCtx;
	AVStream *audioStream;
	AVStream *videoStream;
	AVCodec *videoCodec;
	AVCodec *audioCodec;
	AVCodecContext *videoCC;
	AVCodecContext *audioCC;
	//for IO
	AVIOContext *ioCtx;
	uint8_t ioBuffer[IO_BUFFER_SIZE];
	//for video
	AVFrame *videoFrame;
	AVFrame *audioFrame;
	//packet
	AVPacket *videoPkt;
	AVPacket *audioPkt;
	
	//
	int videoFramePts;
	int videoPktPts;
	int audioFramePts;
	int audioPktPts;
	int remainedAudioSize;
	AVFrame* audioTmpFrame;

	boost::shared_mutex encoderMtx;
	bool stopped;
};
