#pragma once
#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "EncoderCQ.hpp"
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
enum EncoderState {
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
        string filePath;
        string videoCodecName;
        string audioCodecName;
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
	bool isSilent;
};

class Encoder {
public:
	Encoder();
	~Encoder();
	EncoderState initialize(EncoderContext eCtx, void* opaque, int (*encodedPacketCallback)(void*, uint8_t*, int), void (*encoderStateCallback)(EncoderState), uint8_t* (*requestVideoCallback)(int*,int*));
	EncoderState stop();
	bool isStop();

	void setSilent(bool silent);
	//bool encodeVideo(uint8_t* data, int size);
	bool encodeAudio(uint8_t* data, int size);
private:
	//function
	void loopStop(EncoderState state);
	void encodeVideoLoop(uint64_t nano, bool onlyVideo);
	bool encodeVideoImpl(EncoderElement element);
	void encodeAudioLoop(int fps, bool onlyAudio);
	void encodeAudioImpl(EncoderElement element);	

	//variables
	void (*encStateCallback)(EncoderState);

	//request callback
	uint8_t* (*requestVideo)(int*,int*);

	//
	void* opaque;
	int (*encodedPacketCallback)(void*, uint8_t*, int);

	struct EncoderContext eCtx;
	AVDictionary* dict;

	//
	EncoderCQ videoCQ;
	EncoderCQ audioCQ;

	//
	thread videoThread;
	thread audioThread;

	//
	struct timeval videoSrtTs;
	struct timeval audioSrtTs;

	//
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
	int audioBytes;

	boost::shared_mutex encoderMtx;
	boost::shared_mutex syncMtx;
	bool stopped;
};
