#pragma once
#include <iostream>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
using namespace std;

enum QUEUE_MSG {QUEUE_SUCCESS,QUEUE_STOP,QUEUE_NOELEMENT,QUEUE_FULL};

struct EncoderElement {
	uint8_t* data;
	int size;
	bool fake;	

	EncoderElement() {}

	EncoderElement(uint8_t* m_data, int m_size, bool m_fake) {
		data = m_data;
		size = m_size;
		fake = m_fake;
	}
};

class EncoderCQ {
public:
	EncoderCQ(bool mIsWait);
	EncoderCQ(bool mIsWait, int size);
	~EncoderCQ();
	void clear();
	int size();
	QUEUE_MSG enqueue(EncoderElement element);
	QUEUE_MSG dequeue(EncoderElement& element);
	void start();
	void stop();
	
private:
	int front;
	int rear;
	int maxsize;
	boost::mutex mutex;
	boost::condition_variable cond;
	EncoderElement* queue;
	bool isStop;
	bool isWait;
};
