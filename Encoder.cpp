#include "Encoder.hpp"

Encoder::Encoder() : videoCQ(true), audioCQ(true) {
	av_register_all();
	stopped = true;
}


Encoder::~Encoder() {
	stop();
}

EncoderState Encoder::initialize(EncoderContext mEncCtx, void* mOpaque, int (*mEncodedPacketCallback)(void*, uint8_t*, int), void (*encoderStateCallback)(EncoderState), uint8_t* (*requestVideoCallback)(int*,int*)) {
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
	opaque = mOpaque;
	encodedPacketCallback = mEncodedPacketCallback;
	encStateCallback = encoderStateCallback;
	requestVideo = requestVideoCallback;	

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
		videoStream->id = 0;

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
		audioStream->id = 1;

                audioCC = avcodec_alloc_context3(audioCodec);
                if(!audioCC)
                        return AUDIO_CODEC_CONTEXT_ERROR;

                audioCC->bit_rate = eCtx.audioBitrate;
                audioCC->sample_rate = eCtx.samplerate;
		audioStream->time_base = (AVRational){1, audioCC->sample_rate};
		audioCC->channel_layout = eCtx.channel_layout;
                audioCC->channels = eCtx.channels;
		audioCC->sample_fmt = eCtx.outSmpFmt;

		switch(audioCC->sample_fmt) {
		case AV_SAMPLE_FMT_U8:
			audioBytes = 1;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			audioBytes = 2;
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
		case AV_SAMPLE_FMT_FLTP:
			audioBytes = 4;
			break;
		}	

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
		av_dict_set(&dict, "preset", "llhp", 0);
		av_dict_set(&dict, "profile", "main", 0);
		av_dict_set(&dict, "level", "3.1", 0);
		av_dict_set(&dict, "rc", "vbr", 0);
		av_dict_set(&dict, "tune", "zerolatency", 0);

		if(avcodec_open2(videoCC, videoCodec, &dict) < 0)
                        return VIDEO_CODEC_OPEN_ERROR;

		av_dict_free(&dict);
		dict = NULL;

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
		audioFrame->sample_rate = audioCC->sample_rate;

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
		audioTmpFrame->sample_rate = audioFrame->sample_rate;
		printf("tmp nb samples size is %d\n", audioFrame->nb_samples);

		ret = av_frame_get_buffer(audioTmpFrame, 0);
		if(ret < 0)
			return AUDIO_ALLOC_FRAME_BUFFER_ERROR;	
        }

        ioCtx = avio_alloc_context(ioBuffer, IO_BUFFER_SIZE, 1, opaque, NULL, encodedPacketCallback, NULL);
        fmtCtx->pb = ioCtx;

        //if(avio_open(&fmtCtx->pb, eCtx.filePath.c_str(), AVIO_FLAG_READ_WRITE) < 0)
        //        return AVIO_OPEN_ERROR;

	av_dump_format(fmtCtx, 0, eCtx.filePath.c_str(), 1);

	av_dict_set(&dict, "movflags", "+frag_keyframe+empty_moov+default_base_moof", 0);
	av_dict_set(&dict, "frag_duration", "40", 0);
	//av_dict_set(&dict, "frag_size", "65536" , 0);	

	if(avformat_write_header(fmtCtx, &dict) < 0)
		return WRITE_HEADER_ERROR;	

	av_dict_free(&dict);
	dict = NULL;
	
	if(eCtx.videoCodecName.compare("none") != 0)
		videoThread = thread(&Encoder::encodeVideoLoop, this, (uint64_t)(pow(10.0, 9.0) / videoCC->time_base.den));
	if(eCtx.audioCodecName.compare("none") != 0)
		audioThread = thread(&Encoder::encodeAudioLoop, this);

	return SUCCESS;
}

EncoderState Encoder::stop() {
	boost::unique_lock<boost::shared_mutex> lock(encoderMtx);
	if(stopped)
		return ALREADY_STOP_ERROR;	

	stopped = true;
	if(videoThread.joinable()) {
		cout << "encoder video thread wait" << endl;
		videoCQ.stop();
		videoThread.join();
		cout << "encoder video join end" << endl;
	}
	cout << "encoder audio thread joinable" << endl;
	if(audioThread.joinable()) {
		cout << "encoder audio thread wait" << endl;
		audioCQ.stop();
		audioThread.join();
		cout << "encoder audio thread join end" << endl;
	}
	cout << "encoder audio check complete" << endl;

        av_write_trailer(fmtCtx);
	//cout << "free videoFrame" << endl;
        if(videoFrame != NULL) {
                av_frame_free(&videoFrame);
                videoFrame = NULL;
        }
	//cout << "free audioFrame" << endl;
        if(audioFrame != NULL) {
                av_frame_free(&audioFrame);
                audioFrame = NULL;
        }
	//cout << "free audioTmpFrame" << endl;
	if(audioTmpFrame != NULL) {
		av_frame_free(&audioTmpFrame);
		audioTmpFrame = NULL;
	}	
	//cout << "free videoPkt" << endl;
        if(videoPkt != NULL) {
                av_packet_free(&videoPkt);
                videoPkt = NULL;
        }
	//cout << "free audioPkt" << endl;
        if(audioPkt != NULL) {
                av_packet_free(&audioPkt);
                audioPkt = NULL;
        }
	//cout << "free videoCC" << endl;
        if(videoCC != NULL) {
                avcodec_free_context(&videoCC);
                videoCC = NULL;
        }
	//cout << "free audioCC" << endl;
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
	//cout << "fmtCtx" << endl;
        if(fmtCtx != NULL) {
		avformat_free_context(fmtCtx);
                fmtCtx = NULL;
        }
	//cout << "swsCtx" << endl;
	if(swsCtx != NULL) {
		sws_freeContext(swsCtx);
		swsCtx = NULL;
	}
	//cout << "swrCtx" << endl;
	if(swrCtx != NULL) {
		swr_free(&swrCtx);
		swrCtx = NULL;
	}

	//cout << "dict" << endl;
	if(dict != NULL) {
		av_dict_free(&dict);
		dict = NULL;
	}
	//cout << "stop end" << endl;
	cout << "free encoder complete" << endl;
	
	return SUCCESS;
}

bool Encoder::isStop() {
	boost::unique_lock<boost::shared_mutex> lock(encoderMtx);
	return stopped;
}

void Encoder::encodeVideoLoop(uint64_t nano) {
	cout << "nano is " << nano << endl;
	while(!stopped) {
		while(eCtx.isSilent && !stopped) {
			thread([this]() {
				int width, height;
				EncoderElement element;
				element.data = requestVideo(&width, &height);
				element.size = width*height*3;
				if(width != eCtx.width || height != eCtx.height) {
					eCtx.width = width;
					eCtx.height = height;
					stop();
					initialize(eCtx, opaque, encodedPacketCallback, encStateCallback, requestVideo);
				}

				EncoderElement audioElement;
				audioElement.size = audioBytes * audioCC->time_base.den / videoCC->time_base.den;
				audioElement.data = new uint8_t[audioElement.size];
				audioElement.fake = true;
				memset(audioElement.data, 0, audioElement.size);
				audioCQ.enqueue(audioElement);
				
				if(!encodeVideo(element))
					return;
			}).detach();
			this_thread::sleep_for(chrono::nanoseconds(nano));
		}
		
		videoCQ.start();
		while(!eCtx.isSilent && !stopped) {
			EncoderElement element;
			QUEUE_MSG msg = videoCQ.dequeue(element);
			if(msg == QUEUE_STOP)
                        	break;
		
			if(!encodeVideo(element))
				return;
		}
		videoCQ.stop();
	}
}

bool Encoder::encodeVideo(EncoderElement element) {
	uint8_t* inData[1] = { element.data };
	int inLineSize[1] = { 3*eCtx.width };

	sws_scale(swsCtx, inData, inLineSize, 0, eCtx.height, videoFrame->data, videoFrame->linesize);
	delete element.data;
	videoFrame->pts = videoFramePts;
	int ret = avcodec_send_frame(videoCC, videoFrame);
	if(ret < 0) {
		loopStop(VIDEO_ENCODE_ERROR);
		return false;
	}

	videoFramePts++;
	while(ret >= 0) {
		ret = avcodec_receive_packet(videoCC, videoPkt);
		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if(ret < 0) {
			loopStop(VIDEO_RECEIVE_PACKET_ERROR);
			return false;
		}

		//videoPkt->duration = 1000 / videoCC->time_base.den;
		//videoPkt->pts = videoPkt->dts = videoPkt->duration * videoPktPts;
		//videoPktPts++;
		//cout << "video time is " << (double)videoPktPts / videoCC->time_base.den << endl;

		av_packet_rescale_ts(videoPkt, videoCC->time_base, videoStream->time_base);
		cout << "video time is " << videoPkt->pts * 0.000078125 << endl;
		videoPkt->stream_index = videoStream->index;
		if(av_interleaved_write_frame(fmtCtx, videoPkt) != 0) {
			loopStop(VIDEO_WRITE_FRAME_ERROR);
			return false;
		}

		av_packet_unref(videoPkt);
	}

	return true;
}

/*
bool Encoder::encodeVideo(uint8_t* data, int size) {
	//cout << "enqueue video" << endl;
	boost::shared_lock<boost::shared_mutex> lock(encoderMtx);
	if(stopped)
		return !stopped;

	//cout << "enqueue video process" << endl;
	uint8_t* copyData = new uint8_t[size];
	memcpy(copyData, data, size);
	
	QUEUE_MSG msg = videoCQ.enqueue(EncoderElement(copyData, size));
	if(msg == QUEUE_FULL) {
		cout << "videoCQ is full!!" << endl;
		delete copyData;
	}
	//cout << "enqueue video end" << endl;
}

void Encoder::encodeVideoLoop() {
	videoCQ.start();

	while(!stopped) {
		EncoderElement element;
		QUEUE_MSG msg = videoCQ.dequeue(element);
		if(msg == QUEUE_STOP)
			break;
	
		if(eCtx.isSilent) {
			EncoderElement audioElement;
			audioElement.size = audioBytes * audioCC->time_base.den / videoCC->time_base.den;
			audioElement.data = new uint8_t[audioElement.size];
			memset(audioElement.data, 0, audioElement.size);
			audioCQ.enqueue(audioElement);
		}
		else {
			double videoTime = ((double)videoFramePts / videoCC->time_base.den);
			double audioTime = ((double)audioFramePts / audioCC->time_base.den);
			double syncDiff = videoTime - audioTime;
			if(syncDiff > 0.1) {
				EncoderElement audioElement;
				audioElement.size = audioBytes * audioCC->time_base.den * syncDiff;
				audioElement.data = new uint8_t[audioElement.size];
				memset(audioElement.data, 0, audioElement.size);
				audioCQ.enqueue(audioElement);
			}
		}
		
		uint8_t* inData[1] = { element.data };
		int inLineSize[1] = { 3*eCtx.width };
	
        	sws_scale(swsCtx, inData, inLineSize, 0, eCtx.height, videoFrame->data, videoFrame->linesize);
		delete element.data;
		videoFrame->pts = videoFramePts;
		int ret = avcodec_send_frame(videoCC, videoFrame);
        	if(ret < 0) {
			loopStop(VIDEO_ENCODE_ERROR);
			return;
		}
		videoFramePts++;

        	while(ret >= 0) {
                	ret = avcodec_receive_packet(videoCC, videoPkt);
                	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        	break;
                	else if(ret < 0) {
				loopStop(VIDEO_RECEIVE_PACKET_ERROR);
                        	return;
			}

			videoPkt->duration = 1000 / videoCC->time_base.den;
			videoPkt->pts = videoPkt->dts = videoPkt->duration * videoPktPts;
			videoPktPts++;
			cout << "video time is " << (double)videoPktPts / videoCC->time_base.den << endl;
			
			//av_packet_rescale_ts(videoPkt, videoCC->time_base, videoStream->time_base);
                	videoPkt->stream_index = videoStream->index;
                	if(av_interleaved_write_frame(fmtCtx, videoPkt) != 0) {
				loopStop(VIDEO_WRITE_FRAME_ERROR);
                        	return;
			}
	
			av_packet_unref(videoPkt);
        	}
	}
	videoCQ.clear();
}
*/

void Encoder::setSilent(bool silent) {
	eCtx.isSilent = silent;
}

bool Encoder::encodeAudio(uint8_t* data, int size) {
        boost::shared_lock<boost::shared_mutex> lock(encoderMtx);
        if(stopped)
                return !stopped;

	uint8_t* copyData = new uint8_t[size];
	memcpy(copyData, data, size);

	//cout << "encode audio" << endl;	
	QUEUE_MSG msg = audioCQ.enqueue(EncoderElement(copyData, size, false));
	if(msg == QUEUE_FULL) {
		delete copyData;
		cout << "audioCQ is full!!" << endl;
	}
}

void Encoder::encodeAudioLoop() {
	audioCQ.start();

	struct timespec start, end;
	bool silent = eCtx.isSilent;
	int64_t videoSyncPts=0;
	while(!stopped) {
		EncoderElement element;
		//silent인 경우, qemu에서 데이터를 주지 않으므로, 임의로 생성해야함
		//때문에, video와 audio의 sync를 별개로 처리할 경우, 타이밍을 맞추기 어려우므로
		//video의 타이밍을 이용하여야 함.
		QUEUE_MSG msg = audioCQ.dequeue(element);
		if(msg == QUEUE_STOP)
			break;

		if(silent != eCtx.isSilent && element.fake != eCtx.isSilent) {

			double videoTime = ((double)videoFramePts / videoCC->time_base.den);
			double audioTime = ((double)audioFramePts / audioCC->time_base.den);
                	double syncDiff = videoTime - audioTime;
	
			EncoderElement audioElement;
			audioElement.size = element.size + audioBytes * audioCC->time_base.den * syncDiff;
			audioElement.data = new uint8_t[audioElement.size];
			memcpy(audioElement.data, element.data, element.size);
			memset(audioElement.data + element.size, 0, audioElement.size - element.size);
			
			delete element.data;
			element = audioElement;
			silent = eCtx.isSilent;
		}

		//cout << "audio dequeue" << endl;

		/*
		audioFrame->data[0] = element.data;
		audioFrame->nb_samples = element.size;
		audioFrame->pts = audioFramePts;
		int ret = avcodec_send_frame(audioCC, audioFrame);
		if(ret < 0)
                	return AUDIO_ENCODE_ERROR;
        	audioFramePts+=size;

        	while(ret >= 0) {
                	ret = avcodec_receive_packet(audioCC, audioPkt);
                	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        	break;
                	else if(ret < 0) {
				loopStop(AUDIO_RECEIVE_PACKET_ERROR);
                        	return;
			}

                	//audioPkt->duration = 1000 * audioCC->frame_size / audioCC->time_base.den;
                	//audioPkt->pts = audioPkt->dts = audioPkt->duration * audioPktPts;
                	//audioPktPts++;

                	av_packet_rescale_ts(audioPkt, audioCC->time_base, audioStream->time_base);
                	audioPkt->stream_index = audioStream->index;
                	if(av_interleaved_write_frame(fmtCtx, audioPkt) != 0) {
				loopStop(AUDIO_WRITE_FRAME_ERROR);
				return;
			}

                	av_packet_unref(audioPkt);
        	}
		*/

		uint8_t* data = element.data;
		int size = element.size;
		int encodedSize = 0;
		while(encodedSize != size) {
			int restFrameSize = audioTmpFrame->nb_samples * audioBytes - remainedAudioSize;
			int restDataSize = size - encodedSize;

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
			//cout << "encoded Size is " << encodedSize << endl;	

			//int dstNbSamples = av_rescale_rnd(swr_get_delay(swrCtx, audioCC->sample_rate) + audioTmpFrame->nb_samples,
			//					audioCC->sample_rate, audioCC->sample_rate, AV_ROUND_UP);
			//int ret = swr_convert(swrCtx, audioFrame->data, dstNbSamples, (const uint8_t**)audioTmpFrame->data, audioTmpFrame->nb_samples);
			//if(ret < 0)
			//	return AUDIO_SWR_CONVERT_ERROR;
		
			//cout << "swr_convert" << endl;
			audioTmpFrame->pts = audioFramePts;
			if(!eCtx.isSilent) {
				int64_t syncPts = av_rescale_q(audioFramePts, audioCC->time_base, videoCC->time_base);
				if(syncPts - videoSyncPts > (int64_t)videoCC->time_base.den / audioCC->time_base.den) {
					EncoderElement videoElement;
					int width, height;
					videoElement.data = requestVideo(&width, &height);
					videoElement.size = width*height*3;
					videoCQ.enqueue(videoElement);

					videoSyncPts += (int64_t)videoCC->time_base.den / audioCC->time_base.den;
				}
			}
			else
				videoSyncPts = av_rescale_q(audioFramePts, audioCC->time_base, videoCC->time_base);

        		int ret = avcodec_send_frame(audioCC, audioTmpFrame);
        		if(ret < 0) {
				loopStop(AUDIO_ENCODE_ERROR);
                		return;
			}

			audioFramePts += audioTmpFrame->nb_samples;//dstNbSamples;
        		while(ret >= 0) {
                		ret = avcodec_receive_packet(audioCC, audioPkt);
                		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        		break;
                		else if(ret < 0) {
					loopStop(AUDIO_RECEIVE_PACKET_ERROR);
                        		return;
				}

				//audioPkt->duration = 1000 * audioTmpFrame->nb_samples / audioCC->time_base.den;
				//audioPkt->pts = audioPkt->dts = audioPktPts;
				//audioPktPts += audioTmpFrame->nb_samples;
				//cout << "audio time is " << (double)audioPktPts / audioCC->time_base.den << endl;
			
				av_packet_rescale_ts(audioPkt, audioCC->time_base, audioStream->time_base);
	  			cout << "audio time is " << (double)audioPkt->pts / audioCC->time_base.den << endl;
				audioPkt->stream_index = audioStream->index;	
				if(av_interleaved_write_frame(fmtCtx, audioPkt) != 0) {
					loopStop(AUDIO_WRITE_FRAME_ERROR);
					return;
				}

                		av_packet_unref(audioPkt);
        		}
		}
		delete data;
	}
	audioCQ.clear();
}

void Encoder::loopStop(EncoderState state) {
	thread(&Encoder::stop, this);
	encStateCallback(state);
}
