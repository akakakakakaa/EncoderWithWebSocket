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

enum ClbState{START_STREAMING,STOP_STREAMING, STOP_PROGRAM};
#ifdef __cplusplus
extern "C" {
#endif
	extern void clbStart(int collaboPort, int agentPort, const char* meetSeq, const char* password,
                	void (*mouseCallback)(int, int, int),
                	void (*keyboardCallback)(int, bool),
			void (*stateCallback)(enum ClbState));

	extern void clbStop(void);	

	//extern void startEncoder(struct ClbContext mClbCtx);
	extern void startStreaming(struct ClbContext mClbCtx, uint8_t* (*requestVideoCallback)(int*,int*));
	extern void stopStreaming(void);

	//extern bool encodeVideo(uint8_t* data, int size);
	extern void setSilent(bool silent);
	extern bool encodeAudio(uint8_t* data, int size);
#ifdef __cplusplus
}
#endif
