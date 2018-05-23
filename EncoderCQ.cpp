#include "EncoderCQ.hpp"

EncoderCQ::EncoderCQ(bool mIsWait) {
	front = 0;
	rear = 0;
	maxsize = 40;
	isStop = false;
	isWait = mIsWait;
	queue = new EncoderElement[maxsize];
}

EncoderCQ::EncoderCQ(bool mIsWait, int size) {
	front = 0;
	rear = 0;
	maxsize = size;
	isStop = false;
	isWait = mIsWait;
	queue = new EncoderElement[maxsize];
}

EncoderCQ::~EncoderCQ() {
	clear();

	delete queue;
}

void EncoderCQ::clear() {
	//cout << "encodercq clear called" << endl;
	boost::unique_lock<boost::mutex> enqueueLock(enqueueMtx);
	boost::unique_lock<boost::mutex> dequeueLock(dequeueMtx);
	//cout << "encodercq clear lock" << endl;

	//cout << "front is : " << front << " rear is : " << rear << endl;
        if(rear >= front)
                for(int i=front; i<rear; i++)
                        delete queue[i].data;
        else {
                for(int i=0; i<rear; i++)
                        delete queue[i].data;
                for(int i=front; i<maxsize; i++)
                        delete queue[i].data;
        }

	rear = 0;
	front = 0;
}

int EncoderCQ::size() {
	if(rear >= front)
		return rear - front;
	else
		return maxsize + rear - front;
}

QUEUE_MSG EncoderCQ::enqueue(EncoderElement element) {
	boost::unique_lock<boost::mutex> lock(enqueueMtx);
	if(isStop)
		return QUEUE_STOP;	

	//cout << "enqueue = " << "front : " << front << " rear : " << rear << endl;
	if(front == rear+1)
		return QUEUE_FULL;

	rear = (rear == maxsize) ? 0 : (rear + 1);

	queue[rear-1] = element;	
	cond.notify_one();
	return QUEUE_SUCCESS;
}

QUEUE_MSG EncoderCQ::dequeue(EncoderElement& element) {
	//cout << "dequeue = " << "front : " << front << " rear : " << rear << endl;
	boost::unique_lock<boost::mutex> lock(dequeueMtx);
	if(front == rear) {
		//cout << "wait" << endl;
		if(!isWait)
			return QUEUE_NOELEMENT;

		if(!isStop)
			cond.wait(lock);
	}

	if(isStop)
		return QUEUE_STOP;

	front = (front == maxsize) ? 0 : (front + 1);
	element = queue[front-1];

	return QUEUE_SUCCESS;
}

void EncoderCQ::start() {
	//boost::unique_lock<boost::mutex> lock(mutex);
	isStop = false;
}

void EncoderCQ::stop() {
	//boost::unique_lock<boost::mutex> lock(mutex);
	cond.notify_one();
	isStop = true;
}
