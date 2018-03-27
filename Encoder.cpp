#include "Encoder.hpp"

Encoder::Encoder() {
	av_register_all();
	stopped = true;
}


Encoder::~Encoder() {
	stop();
}

EncoderError Encoder::initialize(EncoderContext mEncCtx, void* opaque, int (*encodedPacketCallback)(void*, uint8_t*, int)) {
	boost::unique_lock<boost::shared_mutex> lock(encoderMtx);
	if(!stopped) {
		cout << "encoder already initialized. ignore this." << endl;		
		return ALREADY_INIT_ERROR;
	}

	stopped = false;

	fmtCtx = NULL;
	videoStream = NULL;
	audioStream = NULL;
	ioCtx = NULL;
	videoFrame = NULL;
	audioFrame = NULL;
	videoPkt = NULL;
	audioPkt = NULL;
	swsCtx = NULL;
	swrCtx = NULL;
	dict = NULL;
	videoFramePts = 0;
	videoPktPts = 0;
	audioFramePts = 0;
	audioPktPts = 0;
	remainedAudioSize = 0;
	audioTmpFrame = NULL;	

	//
	eCtx = mEncCtx;

	//
	av_dict_set(&dict, "preset", "lossless", 0);
	av_dict_set(&dict, "profile", "main", 0);
	av_dict_set(&dict, "level", "3.1", 0);
	av_dict_set(&dict, "rc", "vbr", 0);
	av_dict_set(&dict, "movflags", "+frag_keyframe+empty_moov+default_base_moof", 0);
	av_dict_set(&dict, "frag_duration", "40", 0);
	av_dict_set(&dict, "tune", "zerolatency", 0);
	//
        avformat_alloc_output_context2(&fmtCtx, NULL, NULL, eCtx.filePath.c_str());
        if(!fmtCtx)
                return FORMAT_CONTEXT_ERROR;

        AVOutputFormat *outFmt = fmtCtx->oformat;
        if(outFmt->video_codec != AV_CODEC_ID_NONE && eCtx.videoCodecName.compare("none") != 0) {
                videoCodec = avcodec_find_encoder_by_name(eCtx.videoCodecName.c_str());
                if(!videoCodec)
                        return VIDEO_CODEC_ERROR;

		videoStream = avformat_new_stream(fmtCtx, NULL);
		if(!videoStream)
			return VIDEO_NEW_STREAM_ERROR;
		videoStream->id = fmtCtx->nb_streams-1;

                videoCC = avcodec_alloc_context3(videoCodec);
                if(!videoCC)
                        return VIDEO_CODEC_CONTEXT_ERROR;
                videoCC->width = eCtx.width;
                videoCC->height = eCtx.height;
		videoStream->time_base = (AVRational){1, eCtx.framerate};
                videoCC->time_base = videoStream->time_base;
		videoCC->gop_size = eCtx.keyFramerate;
                videoCC->bit_rate = eCtx.videoBitrate;
                videoCC->codec_type = AVMEDIA_TYPE_VIDEO;
		videoCC->pix_fmt = eCtx.outPixFmt;

        	swsCtx = sws_getContext(videoCC->width, videoCC->height, eCtx.inPixFmt, videoCC->width, videoCC->height, videoCC->pix_fmt, 0, 0, 0, 0);
		if(!swsCtx)
			return VIDEO_SWS_GET_CONTEXT_ERROR;
        }

        if(outFmt->audio_codec != AV_CODEC_ID_NONE && eCtx.audioCodecName.compare("none") != 0) {
                audioCodec = avcodec_find_encoder_by_name(eCtx.audioCodecName.c_str());
                if(!audioCodec)
                        return AUDIO_CODEC_ERROR;

		audioStream = avformat_new_stream(fmtCtx, NULL);
		if(!audioStream)
			return AUDIO_NEW_STREAM_ERROR;
		audioStream->id = fmtCtx->nb_streams-1;

                audioCC = avcodec_alloc_context3(audioCodec);
                if(!audioCC)
                        return AUDIO_CODEC_CONTEXT_ERROR;

                audioCC->bit_rate = eCtx.audioBitrate;
                audioCC->sample_rate = eCtx.samplerate;
		audioStream->time_base = (AVRational){1, audioCC->sample_rate};
		audioCC->channel_layout = eCtx.channel_layout;
                audioCC->channels = eCtx.channels;
		audioCC->sample_fmt = eCtx.outSmpFmt;

        //
		swrCtx = swr_alloc();
		if(!swrCtx)
			return AUDIO_SWR_ALLOC_ERROR;
		av_opt_set_int(swrCtx, "in_channel_layout", audioCC->channel_layout, 0);
		av_opt_set_int(swrCtx, "in_sample_rate", audioCC->sample_rate, 0);
		av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", eCtx.inSmpFmt, 0);
		
		av_opt_set_int(swrCtx, "out_channel_layout", audioCC->channel_layout, 0);
		av_opt_set_int(swrCtx, "out_sample_rate", audioCC->sample_rate, 0);
		av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", audioCC->sample_fmt, 0);

		if(swr_init(swrCtx) < 0)
			return AUDIO_SWR_INIT_ERROR;	
        }
	
	if(fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
		if(videoStream)
			videoCC->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		if(audioStream)
			audioCC->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

        if(videoStream) {
		if(avcodec_open2(videoCC, videoCodec, &dict) < 0)
                        return VIDEO_CODEC_OPEN_ERROR;

		int ret = avcodec_parameters_from_context(videoStream->codecpar, videoCC);
		if(ret < 0)
			return VIDEO_CODECPAR_ERROR;		

		videoFrame = av_frame_alloc();
                if(!videoFrame)
                        return VIDEO_ALLOC_FRAME_ERROR;
                videoFrame->format = videoCC->pix_fmt;
                videoFrame->width = videoCC->width;
                videoFrame->height = videoCC->height;

		ret = av_frame_get_buffer(videoFrame, 32);
		if(ret < 0)
			return VIDEO_ALLOC_FRAME_BUFFER_ERROR;			

                videoPkt = av_packet_alloc();
                if(!videoPkt)
                        return VIDEO_ALLOC_PACKET_ERROR;
        }
        if(audioStream) {
                if(avcodec_open2(audioCC, audioCodec, &dict) < 0)
                        return AUDIO_CODEC_OPEN_ERROR;

		int ret = avcodec_parameters_from_context(audioStream->codecpar, audioCC);
		if(ret < 0)
			return AUDIO_CODECPAR_ERROR;

                audioFrame = av_frame_alloc();
                if(!audioFrame)
                        return AUDIO_ALLOC_FRAME_ERROR;
  		
		//sample에 frame_size보다 큰 사이즈 들어오면 에러뜨면서 안됨
		if(audioCC->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
			audioFrame->nb_samples = 10000;
		else
			audioFrame->nb_samples = audioCC->frame_size;
		audioFrame->format = audioCC->sample_fmt;
		audioFrame->channel_layout = audioCC->channel_layout;

		ret = av_frame_get_buffer(audioFrame, 0);
		if(ret < 0)
			return AUDIO_ALLOC_FRAME_BUFFER_ERROR;
 
                audioPkt = av_packet_alloc();
                if(!audioPkt)
                        return AUDIO_ALLOC_PACKET_ERROR;

		audioTmpFrame = av_frame_alloc();
		if(!audioTmpFrame)
			return AUDIO_ALLOC_FRAME_ERROR;

		audioTmpFrame->nb_samples = audioFrame->nb_samples;
		audioTmpFrame->format = eCtx.inPixFmt;
		audioTmpFrame->channel_layout = audioFrame->channel_layout;

		ret = av_frame_get_buffer(audioTmpFrame, 0);
		if(ret < 0)
			return AUDIO_ALLOC_FRAME_BUFFER_ERROR;
        }

        ioCtx = avio_alloc_context(ioBuffer, IO_BUFFER_SIZE, 1, opaque, NULL, encodedPacketCallback, NULL);
        fmtCtx->pb = ioCtx;

        //if(avio_open(&fmtCtx->pb, eCtx.filePath.c_str(), AVIO_FLAG_READ_WRITE) < 0)
        //        return AVIO_OPEN_ERROR;

	av_dump_format(fmtCtx, 0, eCtx.filePath.c_str(), 1);

	cout << "write header" << endl;
	if(avformat_write_header(fmtCtx, &dict) < 0)
		return WRITE_HEADER_ERROR;	
	cout << "write header end" << endl;

	return SUCCESS;
}

EncoderError Encoder::stop() {
	boost::unique_lock<boost::shared_mutex> lock(encoderMtx);
	if(stopped) {
		cout << "encoder already stopped. ignore this" << endl;
		return ALREADY_STOP_ERROR;
	}

	stopped = true;

        av_write_trailer(fmtCtx);
	cout << "free videoFrame" << endl;
        if(videoFrame != NULL) {
                av_frame_free(&videoFrame);
                videoFrame = NULL;
        }
	cout << "free audioFrame" << endl;
        if(audioFrame != NULL) {
                av_frame_free(&audioFrame);
                audioFrame = NULL;
        }
	cout << "free audioTmpFrame" << endl;
	if(audioTmpFrame != NULL) {
		av_frame_free(&audioTmpFrame);
		audioTmpFrame = NULL;
	}	
	cout << "free videoPkt" << endl;
        if(videoPkt != NULL) {
                av_packet_free(&videoPkt);
                videoPkt = NULL;
        }
	cout << "free audioPkt" << endl;
        if(audioPkt != NULL) {
                av_packet_free(&audioPkt);
                audioPkt = NULL;
        }
	cout << "free videoCC" << endl;
        if(videoCC != NULL) {
                avcodec_free_context(&videoCC);
                videoCC = NULL;
        }
	cout << "free audioCC" << endl;
        if(audioCC != NULL) {
                avcodec_free_context(&audioCC);
                audioCC = NULL;
        }
	/*
	printf("free videoStream" << endl;
        if(videoStream != NULL) {
                av_freep(&videoStream);
                videoStream = NULL;
        }
	printf("free audioStream" << endl;
        if(audioStream != NULL) {
                av_freep(&audioStream);
                audioStream = NULL;
        }
	*/
	//printf("free fmtCtx pb\n");
	/*
        if(ioCtx != NULL) {
		//avio_closep(&fmtCtx->pb) free error why?
		//avio_close(fmtCtx->pb);
		//if only alloc avio, use av_freep maybe
		av_freep(&ioCtx->buffer);
		av_freep(&ioCtx);
                ioCtx = NULL;
        }
	*/
	cout << "fmtCtx" << endl;
        if(fmtCtx != NULL) {
		avformat_free_context(fmtCtx);
                fmtCtx = NULL;
        }
	cout << "swsCtx" << endl;
	if(swsCtx != NULL) {
		sws_freeContext(swsCtx);
		swsCtx = NULL;
	}
	cout << "swrCtx" << endl;
	if(swrCtx != NULL) {
		swr_free(&swrCtx);
		swrCtx = NULL;
	}

	cout << "dict" << endl;
	if(dict != NULL) {
		av_dict_free(&dict);
		dict = NULL;
	}
	cout << "stop end" << endl;
}

bool Encoder::isStop() {
	boost::unique_lock<boost::shared_mutex> lock(encoderMtx);
	return stopped;
}

EncoderError Encoder::encodeVideo(uint8_t* data, int size) {
	boost::shared_lock<boost::shared_mutex> lock(encoderMtx);
	if(stopped)
		return ENCODER_STOPPED;

	uint8_t* inData[1] = { data };
	int inLineSize[1] = { 3*eCtx.width };
	
        sws_scale(swsCtx, inData, inLineSize, 0, eCtx.height, videoFrame->data, videoFrame->linesize);
	videoFrame->pts = videoFramePts;
	int ret = avcodec_send_frame(videoCC, videoFrame);
        if(ret < 0)
                return VIDEO_ENCODE_ERROR;
	videoFramePts++;

        while(ret >= 0) {
                ret = avcodec_receive_packet(videoCC, videoPkt);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                else if(ret < 0)
                        return VIDEO_RECEIVE_PACKET_ERROR;

		//videoPkt->duration = 1000 / videoCC->time_base.den;
		//videoPkt->pts = videoPkt->dts = videoPkt->duration * videoPktPts;
		//videoPktPts++;

		av_packet_rescale_ts(videoPkt, videoCC->time_base, videoStream->time_base);
                videoPkt->stream_index = videoStream->index;
                if(av_interleaved_write_frame(fmtCtx, videoPkt) != 0)
                        return VIDEO_WRITE_FRAME_ERROR;
	
                av_packet_unref(videoPkt);
        }

        return SUCCESS;
}

EncoderError Encoder::encodeAudio(uint8_t* data, int size) {
	boost::shared_lock<boost::shared_mutex> lock(encoderMtx);
	if(stopped)
		return ENCODER_STOPPED;

	int encodedSize = 0;
	cout << "data size is " << size << endl;
	while(encodedSize != size) {
		int restFrameSize = audioTmpFrame->nb_samples - remainedAudioSize;
		int restDataSize = size - encodedSize;
		cout << "aa" << endl;

		if(restDataSize >= restFrameSize) {
			memcpy(audioTmpFrame->data[0] + remainedAudioSize, data + encodedSize, restFrameSize);
			remainedAudioSize = 0;
			encodedSize += restFrameSize;
		}
		else {
			memcpy(audioTmpFrame->data[0] + remainedAudioSize, data + encodedSize, restDataSize);
			remainedAudioSize += restDataSize;
			break;
		}	
		cout << "encoded Size is " << encodedSize << endl;	

		int dstNbSamples = av_rescale_rnd(swr_get_delay(swrCtx, audioCC->sample_rate) + audioTmpFrame->nb_samples,
							audioCC->sample_rate, audioCC->sample_rate, AV_ROUND_UP);
		int ret = swr_convert(swrCtx, audioFrame->data, dstNbSamples, (const uint8_t**)audioTmpFrame->data, audioTmpFrame->nb_samples);
		if(ret < 0)
			return AUDIO_SWR_CONVERT_ERROR;
		
		cout << "swr_convert" << endl;	
		audioFrame->pts = av_rescale_q(audioFramePts, (AVRational){1, audioCC->sample_rate}, audioCC->time_base);
        	ret = avcodec_send_frame(audioCC, audioFrame);
        	if(ret < 0)
                	return AUDIO_ENCODE_ERROR;

		audioFramePts += dstNbSamples;
        	while(ret >= 0) {
                	ret = avcodec_receive_packet(audioCC, audioPkt);
                	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                	else if(ret < 0)
                        	return AUDIO_RECEIVE_PACKET_ERROR;

			av_packet_rescale_ts(audioPkt, audioCC->time_base, audioStream->time_base);
                	audioPkt->stream_index = audioStream->index;
	               	if(av_interleaved_write_frame(fmtCtx, audioPkt) != 0)
                        	return AUDIO_WRITE_FRAME_ERROR;

                	av_packet_unref(audioPkt);
        	}
	}
	cout << "encoder complete" << endl;
        return SUCCESS;
}
