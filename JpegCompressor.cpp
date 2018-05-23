#include "JpegCompressor.hpp"

JpegCompressor::JpegCompressor() {
	stopped = false;
	fullFlag = false;
}

JpegCompressor::~JpegCompressor() {
	stopped = true;
}
void JpegCompressor::initialize(int fps, int width, int height, int quality, void (*sendImageInitCallback)(int, int), void (*sendImageCallback)(uint8_t*, int), uint8_t* (*requestVideoCallback)(int*,int*)) {
	sendImageInit = sendImageInitCallback;
	sendImage = sendImageCallback;
	requestVideo = requestVideoCallback;

	thread(&JpegCompressor::compressJpegLoop, this, fps, width, height, quality).detach();	
}

void JpegCompressor::compressJpegLoop(int fps, int width, int height, int quality) {
	stopped = false;
	
	uint64_t nano = 40000000;
	cout << "nano is " << nano << endl;
	uint8_t* prevBuf = NULL;
	sendImageInit(width, height);
	int seq=0;
	int rawWidth, rawHeight;
	int minX, minY, maxX, maxY;
	uint8_t* jpeg = NULL;
	long unsigned int size;
	bool isChanged = false;
	while(!stopped) {
		//cout << width << " " << height << endl;
		uint8_t* data = requestVideo(&rawWidth, &rawHeight);
		if(rawWidth != width || rawHeight != height) {
			width = rawWidth;
			height = rawHeight;
			sendImageInit(width, height);
			if(prevBuf != NULL) {
				delete prevBuf;
				prevBuf = NULL;
			}
		}
		if(fullFlag) {
			if(prevBuf != NULL) {
				delete prevBuf;
				prevBuf = NULL;
			}
			fullFlag = false;
		}
		if(prevBuf != NULL) {
			minX=rawWidth;
			minY=rawHeight;
			maxX=0;
			maxY=0;
			isChanged = false;

			for(int i=0; i < rawHeight; i++) {
				for(int j=0; j< rawWidth; j++) {
					if(i == rawHeight - 1 && j == rawWidth - 1) {
						//if((prevBuf + (i*rawWidth*3) + (j*3) != data + (i*rawWidth*3) + (j*3)) ||
						//    prevBuf + (i*rawWidth*3) + (j*3) + 1 != data + (i*rawWidth*3) + (j*3) + 1 ||
						//    prevBuf + (i*rawWidth*3) + (j*3) + 2 != data + (i*rawWidth*3) + (j*3) + 2) {
						//	maxX = width;
						//	maxY = height;
						//}
						break;
					}
					if(*((int*)(prevBuf + (i*rawWidth*3) + (j*3))) ^
					   *((int*)(data    + (i*rawWidth*3) + (j*3)))) {
						isChanged = true;
						int posX = j;
						int posY = i;
						if(posX < minX)
							minX = posX;
						if(posY < minY)
							minY = posY;
						if(posX > maxX)
							maxX = posX;
						if(posY > maxY)
							maxY = posY;
					}
				}
			}
			//cout << "maxX: " << maxX << "maxY: " << maxY << endl;
			maxX++;
			maxY++;
			/*
			if(isChanged) {
				int xDiff = 8-(maxX - minX)%8;
				int yDiff = 8-(maxY - minY)%8;
				if(maxX + xDiff <= rawWidth)
					maxX += xDiff;
				else if(minX 
					minX -= xDiff;
				
				if(maxY + yDiff <= rawHeight)
					maxY += yDiff;
				else
					minY -= yDiff;	
			}
			*/
			//cout << "maxX: " << maxX << "maxY: " << maxY << endl;

			delete[] prevBuf;
		}
		else {
			isChanged = true;
			minX = 0;
			minY = 0;
			maxX = rawWidth;
			maxY = rawHeight;
		}
		prevBuf = data;

		if(isChanged) {
			tjhandle compressor = tjInitCompress();
			cout << "tjCompress start. minX: " << minX << ",minY: " << minY << ",maxX: " << maxX << ",maxY: " << maxY << endl;
			int partialWidth = maxX - minX;
			int partialHeight = maxY - minY;
			int err = tjCompress2(compressor, data, rawWidth, (maxX - minX)*3, rawHeight, TJPF_RGB, &jpeg, minX, minY, partialWidth, partialHeight, &size, TJSAMP_420, quality, TJFLAG_FASTDCT);

			cout << "tjCompress end. size is " << size << endl;
			tjDestroy(compressor);
			if(err < 0 && jpeg != NULL) {
				cout << "jpeg compress error occured" << endl;
				tjFree(jpeg);
				jpeg = NULL;
			}
			else if(jpeg != NULL) {
				
				uint8_t* sendBuf = new uint8_t[1 + sizeof(int) * 5 + size];
				memcpy(sendBuf + 1, &seq, sizeof(int));
				memcpy(sendBuf + 1 + sizeof(int), &minX, sizeof(int));
				memcpy(sendBuf + 1 + sizeof(int)*2, &minY, sizeof(int));
				memcpy(sendBuf + 1 + sizeof(int)*3, &partialWidth, sizeof(int));
				memcpy(sendBuf + 1 + sizeof(int)*4, &partialHeight, sizeof(int));
				memcpy(sendBuf + 1 + sizeof(int)*5, jpeg, (int)size);

				tjFree(jpeg);
				jpeg = NULL;

				sendImage(sendBuf, 1 + sizeof(int) * 5 + size);
				seq++;
			}
		}
		this_thread::sleep_for(chrono::nanoseconds(nano));
	}
}

void JpegCompressor::stop() {
	stopped = true;
}

void JpegCompressor::sendFull() {
	fullFlag = true;
}
