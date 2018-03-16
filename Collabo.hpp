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
	enum ClbPixFmt inPixFmt;
	enum ClbPixFmt outPixFmt;
	enum ClbSmpFmt inSmpFmt;
	enum ClbPixFmt outSmpFmt;
};

enum ClbState{START_ENCODING,STOP_ENCODING};
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

	extern void startEncoder(struct ClbContext mClbCtx);
	extern void stopEncoder(void);

	extern void encodeVideo(uint8_t* data, int size);
	extern void encodeAudio(uint8_t* data, int size);
#ifdef __cplusplus
}
#endif
