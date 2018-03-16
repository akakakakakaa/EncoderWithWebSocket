#pragma once
#include <iostream>
#include <string>
#include <uWS/uWS.h>

class WebSocketServer {
public:
	WebSocketServer();
	~WebSocketServer();
		
	bool initialize(int port, bool isSSL,
		std::function<void(uWS::WebSocket<uWS::SERVER>*)> onOpenPtr,
		std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onMsgPtr,
		std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onClosePtr);
	void run();
	void stop();
	void sendAll(char* msg, size_t length);	

private:
	uWS::Hub hub;
	uWS::Group<uWS::SERVER> *group;
};
