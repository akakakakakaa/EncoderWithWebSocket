#include <stdint.h>

enum ClbPixFmt {
	PIX_FMT_R8G8B8
};

enum ClbSmpFmt {
	SMP_FMT_S16,
	SMP_FMT_S32
};

struct ClbContext {
	bool isVideoEnabled;
	bool isAudioEnabled;
	int fps;
	int width;
	int height;
	int samplerate;
	int channels;
	bool isSilent;
	enum ClbPixFmt inPixFmt;
	enum ClbPixFmt outPixFmt;
	enum ClbSmpFmt inSmpFmt;
	enum ClbPixFmt outSmpFmt;
};

enum ClbState{START_ENCODING,STOP_ENCODING, STOP_PROGRAM};
#ifdef __cplusplus
extern "C" {
#endif
	extern void initVideo(int width, int height, int fps, int kfps, int bitrate);

	extern void initAudio(int samplerate, int bitrate);

	extern void clbStart(int port,
                	void (*mouseCallback)(int, int, int),
                	void (*keyboardCallback)(int, bool),
			void (*stateCallback)(enum ClbState));

	extern void clbStop(void);	

	//extern void startEncoder(struct ClbContext mClbCtx);
	extern void startEncoder(struct ClbContext mClbCtx, uint8_t* (*requestVideoCallback)(int*,int*));
	extern void stopEncoder(void);

	//extern bool encodeVideo(uint8_t* data, int size);
	extern void setSilent(bool silent);
	extern bool encodeAudio(uint8_t* data, int size);
#ifdef __cplusplus
}
#endif
