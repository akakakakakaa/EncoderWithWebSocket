#include "WebSocketServer.hpp"

WebSocketServer::WebSocketServer() {
        group = hub.createGroup<uWS::SERVER>(uWS::PERMESSAGE_DEFLATE);
}

WebSocketServer::~WebSocketServer() {
}

bool WebSocketServer::initialize(int port, bool isSSL,
			std::function<void(uWS::WebSocket<uWS::SERVER>*)> onOpenPtr,
			std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onMsgPtr,
			std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onClosePtr) {

        group->onConnection([=](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
		onOpenPtr(ws);
        });

        group->onMessage([=](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
		onMsgPtr(ws, message, length);
        });

        group->onDisconnection([=](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
        	onClosePtr(ws, message, length);
	});

        if(isSSL) {
                uS::TLS::Context c = uS::TLS::createContext(
                                        "misc/ssl/cert.pem",
                                        "misc/ssl/key.pem",
                                        "1234");
                if(!hub.listen(port, c, 0, group))
                        return false;
        }
        else
                if(!hub.listen(port, nullptr, 0, group))
                        return false;

        return true;
}

void WebSocketServer::run() {
	hub.run();
}

void WebSocketServer::stop() {
	group->close();
	delete group;
}

void WebSocketServer::sendAll(char* msg, size_t length) {
	group->broadcast(msg, length, uWS::OpCode::BINARY);
}
