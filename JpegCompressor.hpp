#include <iostream>
#include <cmath>
#include <functional>
#include <thread>
#include <cstring>
#include <fstream>
#include "turbojpeg.h"
using namespace std;

//resize 기능 추가 필요
class JpegCompressor {
public:
	JpegCompressor();
	~JpegCompressor();
	void initialize(int fps, int width, int height, int quality, void (*sendImageInitCallback)(int, int), void (*sendImageCallback)(uint8_t*, int), uint8_t* (*requestVideoCallback)(int*,int*));
	void compressJpegLoop(int fps, int width, int height, int quality);
	void stop();
	void sendFull();

private:
	bool stopped;
	bool fullFlag;
	void (*sendImageInit)(int, int);
	void (*sendImage)(uint8_t*, int);
	uint8_t* (*requestVideo)(int*,int*);
};

