#include "Collabo.hpp"
#include "WebSocketServer.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/mutex.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <list>
#include "MySQLConnector.hpp"
#include "Encoder.hpp"
using namespace std;
using JSON = nlohmann::json;

#define DEBUG 1

//client to server msg
enum CTSMSG{MOUSE,KEYBOARD,AUTHORITY,CHIEF,SETTING};
//server to client msg
enum STCMSG{INITVIDEO,DECODEVIDEO,AUTHPROCESS,CHIEFPROCESS,REMOTEDESKTOPSETTING,ACCESSMEMBERPROCESS,CONNECTEDMEMBERINFOPROCESS};

struct Client {
        Client(uWS::WebSocket<uWS::SERVER> *_ws) : ws(_ws), chief(false), authority(false), role(-1), mcode(""), mscode(""), custom(""), connectedTime(-1), isFirst(true), isSet(false) {}
        uWS::WebSocket<uWS::SERVER> *ws;
        bool chief;
        bool authority;
        int role;
        string mcode;
        string mscode;
	string custom;
        int connectedTime;
        bool isFirst;
        bool isSet;

        void clear() {
                ws = NULL;
                chief = false;
                authority = false;
                role = -1;
                mcode = "";
                mscode = "";
                connectedTime = -1;
                isFirst = true;
                isSet = false;
        }
};

typedef boost::multi_index_container<
	Client,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_non_unique<
			boost::multi_index::member<Client, uWS::WebSocket<uWS::SERVER>*, &Client::ws>
		>,
		boost::multi_index::ordered_unique<
			boost::multi_index::member<Client, std::string, &Client::mcode>
		>
	>
> ClientList;
typedef ClientList::nth_index<1>::type McodeType;
typedef ClientList::nth_index<0>::type WSType;
struct changeAuth {
        changeAuth(const bool auth) : newAuth(auth) {}

        void operator()(Client& client) {
                client.authority = newAuth;
        }

private:
        bool newAuth;
};

struct changeChief {
        changeChief(const bool chief) : newChief(chief) {}

        void operator()(Client& client) {
                client.chief = newChief;
        }

private:
        bool newChief;
};

struct settingInfo {
        settingInfo(const Client& client) {
		mcode = client.mcode;
		mscode = client.mscode;
		custom = client.custom;
		role = client.role;
		authority = client.authority;
		chief = client.chief;
	}

        void operator()(Client& client) {
		client.mcode = mcode;
		client.mscode = mscode;
		client.custom = custom;
		client.role = role;
		client.authority = authority;
		client.chief = chief;
        }

private:
        string mcode;
	string mscode;
	string custom;
	int role;
	bool authority;
	bool chief;
};

static ClbContext clbCtx;
static Encoder encoder;
static WebSocketServer server;
static ClientList clientList;
static int firstCount=0;
static uint8_t header[10000];
static int headerSize=0;
static void (*clbStateCallback)(enum ClbState);
static boost::shared_mutex sharedMtx;
static boost::mutex encoderMtx;

void sendVideoInitMsg(uWS::WebSocket<uWS::SERVER>* ws) {
        //identifier, width, height, header
        char* msg = new char[1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int) + headerSize];
        msg[0] = INITVIDEO;
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long current = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        memcpy(msg + 1, &current, sizeof(long));
	memcpy(msg + 1 + sizeof(long), &clbCtx.isVideoEnabled, sizeof(bool));
	memcpy(msg + 1 + sizeof(long) + sizeof(bool), &clbCtx.isAudioEnabled, sizeof(bool));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool), &clbCtx.width, sizeof(int));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int), &clbCtx.height, sizeof(int));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int), header, headerSize);

	ws->send(msg, 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int) + headerSize, uWS::OpCode::BINARY);
        delete[] msg;
}

void sendVideoInitMsgAll() {
        //identifier, width, height, header
        char* msg = new char[1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int) + headerSize];
        msg[0] = INITVIDEO;
	struct timeval tp;
	gettimeofday(&tp, NULL);
	long current = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	memcpy(msg + 1, &current, sizeof(long));
	memcpy(msg + 1 + sizeof(long), &clbCtx.isVideoEnabled, sizeof(bool));
	memcpy(msg + 1 + sizeof(long) + sizeof(bool), &clbCtx.isAudioEnabled, sizeof(bool));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool), &clbCtx.width, sizeof(int));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int), &clbCtx.height, sizeof(int));
        memcpy(msg + 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int), header, headerSize);

	WSType& wsType = clientList.get<0>();
	for(WSType::iterator it = wsType.begin(); it != wsType.end(); ++it)
		if((*it).role != -1)
        		(*it).ws->send(msg, 1 + sizeof(long) + sizeof(bool) + sizeof(bool) + sizeof(int) + sizeof(int) + headerSize, uWS::OpCode::BINARY);
        delete[] msg;
}

void sendVideoPktAll(uint8_t* buf, int size) {
        char* msg = new char[1 + sizeof(long) + size];
        msg[0] = DECODEVIDEO;
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long current = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        //cout << current << endl;
        memcpy(msg + 1, &current, sizeof(long));
        memcpy(msg + 1 + sizeof(long), buf, size);

	WSType& wsType = clientList.get<0>();
	for(WSType::iterator it = wsType.begin(); it != wsType.end(); ++it)
		if((*it).role != -1)
			(*it).ws->send(msg, 1 + sizeof(long) + size, uWS::OpCode::BINARY);

        delete[] msg;
}

int readPacket(void* opaque, uint8_t* buf, int size) {
	printf("packet size is %d\n", size);
	WebSocketServer* server = (WebSocketServer*)opaque;
	if(firstCount < 2) {
		memcpy(header+headerSize, buf, size);
		headerSize += size;
		firstCount++;
		if(firstCount == 2)
			sendVideoInitMsgAll();
	}
	else
		sendVideoPktAll(buf, size);
	
	return size;
}

void clbStart(int port,
	void (*mouseCallback)(int, int, int),
	void (*keyboardCallback)(int, bool),
	void (*stateCallback)(enum ClbState)) {
	clbStateCallback = stateCallback;

	//setting mysqlContext
	MySQLContext mCtx;
	mCtx.ip = "163.180.117.108";
	mCtx.id = "ccubedev";
	mCtx.pw = "ccube123#";
	mCtx.schema = "ccbdb";

	MySQLConnector mysqlConnector(mCtx);

	std::function<void(uWS::WebSocket<uWS::SERVER>*)> onOpenPtr =
	[&](uWS::WebSocket<uWS::SERVER>* ws) -> void {
		boost::unique_lock<boost::shared_mutex> lock(sharedMtx);

		Client client(ws);
		clientList.insert(client);	

		if(clientList.size() == 1)
			clbStateCallback(START_ENCODING);
	};

	std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onMsgPtr = 
	[&](uWS::WebSocket<uWS::SERVER>* ws, char* msg, size_t size) -> void {
		boost::shared_lock<boost::shared_mutex> lock(sharedMtx);
		WSType& wsType = clientList.get<0>();
		WSType::iterator wIt = wsType.find(ws);
		Client client = *wIt;

		//cout << "msg is " << msg << endl;
		//msg is {"identifier":4,"mcode":"dGVzdDFAdW5pd2lzLmNvbQ","mscode":"MTUwZnJvbTIwMTgtMDMtMDggMTY6MDA6MDB0bzIwMTgtMDMtMDggMTc6MjU6MDA=","custom":"28"}
		//msg is {"identifier":1,"flag":1,"kCode":40}pd2lzLmNvbQ","mscode":"MTUwZnJvbTIwMTgtMDMtMDggMTY6MDA6MDB0bzIwMTgtMDMtMDggMTc6MjU6MDA=","custom":"28"}
		//제일 긴 메시지의 사이즈를 버퍼의 사이즈로  하고, 그 이후에 계속 쓰는듯, size를 이용해 필요한 부분만 써야할듯
		JSON json = JSON::parse(msg, msg + size);
		int identifier = json["identifier"];
		switch(identifier) {
			case MOUSE: {
				if(client.authority) {
					int flag = json["flag"];
					int x = json["x"];
					int y = json["y"];
					
					//cout << x << " " << y << endl;
					mouseCallback(x, y, flag);
				}
				break;
			}

			case KEYBOARD: {
				if(client.authority) {
					int flag = json["flag"];
					int kCode = json["kCode"];
					keyboardCallback(kCode, flag);
				}
				break;
			}
			case AUTHORITY: {
				string authDst = json["dst"];
				if(client.chief && !client.mcode.compare(authDst)) {
					McodeType& mcodeType = clientList.get<1>();
					McodeType::iterator mIt = mcodeType.find(authDst);

					if(mIt->authority) {
						mcodeType.modify(mIt, changeAuth(false));
						mysqlConnector.changeAuth(authDst.c_str(), false, mIt->mscode.c_str());
					}
					else {
						mcodeType.modify(mIt, changeAuth(true));
						mysqlConnector.changeAuth(authDst.c_str(), true, mIt->mscode.c_str());
					}

					int mcodeSize = mIt->mcode.size();
					char* sendData = new char[6+mcodeSize];
					sendData[0] = AUTHPROCESS;
					sendData[1] = mIt->authority;
					memcpy(sendData+2, &mcodeSize, 4);
					memcpy(sendData+6, mIt->mcode.c_str(), mcodeSize);
					for(McodeType::iterator it = mcodeType.begin(); it != mcodeType.end(); ++it)
						it->ws->send(sendData, 6+mcodeSize, uWS::OpCode::BINARY);
					delete sendData;
				}
				break;
			}

			case CHIEF: {
				string chiefDst = json["dst"];
				if(client.chief && !client.mcode.compare(chiefDst)) {
					McodeType& mcodeType = clientList.get<1>();
					McodeType::iterator srcIt = mcodeType.find(client.mcode);
					McodeType::iterator dstIt = mcodeType.find(chiefDst);

					mcodeType.modify(srcIt, changeChief(false));
					mcodeType.modify(dstIt, changeChief(true));
					mcodeType.modify(dstIt, changeAuth(true));
					mysqlConnector.changeChief(client.mcode.c_str(), chiefDst.c_str(), client.mscode.c_str());

					int srcMcodeSize = srcIt->mcode.size();
					int dstMcodeSize = dstIt->mcode.size();
					char* sendData = new char[9+srcMcodeSize+dstMcodeSize];
					sendData[0] = CHIEFPROCESS;
					memcpy(sendData+1, &srcMcodeSize, 4);
					memcpy(sendData+5, &dstMcodeSize, 4);
					memcpy(sendData+9, client.mcode.c_str(), srcMcodeSize);
					memcpy(sendData+9+srcMcodeSize, client.mcode.c_str(), dstMcodeSize);

					for(McodeType::iterator it = mcodeType.begin(); it != mcodeType.end(); ++it)
						it->ws->send(sendData, 9+srcMcodeSize, uWS::OpCode::BINARY);
					delete sendData;
				}
				break;
			}

			case SETTING: {
				client.mcode = json["mcode"];
				client.mscode = json["mscode"];
				client.custom = json["custom"];

				if(mysqlConnector.isSuited(
					client.ws->getAddress().address,
					client.mcode.c_str(), client.mscode.c_str(),
					client.authority, client.chief, client.role)) {

					McodeType& mcodeType = clientList.get<1>();
					McodeType::iterator mIt = mcodeType.find(client.mcode);

					//if same mcode exists, turn off the other client.
					if(mIt != mcodeType.end())
						mIt->ws->terminate();
					wsType.modify(wIt, settingInfo(client));					
	
					//alert connected info to others	
					int cliMcodeSize = client.mcode.size();
					int cliCustomSize = client.custom.size();
					char* alertConnect = new char[11 + cliMcodeSize + cliCustomSize];
					alertConnect[0] = ACCESSMEMBERPROCESS;
					memcpy(alertConnect+1, &client.authority, 1);
					memcpy(alertConnect+2, &client.chief, 1);
					memcpy(alertConnect+3, &cliMcodeSize, 4);
					memcpy(alertConnect+7, &cliCustomSize, 4);
					memcpy(alertConnect+11, client.mcode.c_str(), cliMcodeSize);
					memcpy(alertConnect+11+cliMcodeSize, client.custom.c_str(), cliCustomSize);
					server.sendAll(alertConnect, 11 + cliMcodeSize + cliCustomSize);
					delete alertConnect;

					//send other client infos to connected client
					int mcodeSizes = 0;
					int customSizes = 0;
					for(McodeType::iterator it = mcodeType.begin(); it != mcodeType.end(); ++it)
						if((*it).role != -1) {
							mcodeSizes += (*it).mcode.size();
							customSizes += (*it).custom.size();
						}

					char* connectedList = new char[1+mcodeSizes+customSizes+10*clientList.size()];
					connectedList[0] = CONNECTEDMEMBERINFOPROCESS;

					int size = 0;
					int memcpyedSize = 1;
					for(McodeType::iterator it = mcodeType.begin(); it != mcodeType.end(); ++it)
						if((*it).role != -1) {
							int mcodeSize = (*it).mcode.size();
							int customSize = (*it).custom.size();
							memcpy(connectedList+memcpyedSize, &(*it).authority, 1);
							memcpy(connectedList+memcpyedSize+1, &(*it).chief, 1);
							memcpy(connectedList+memcpyedSize+2, &mcodeSize, 4);
							memcpy(connectedList+memcpyedSize+6, &customSize, 4);
							memcpy(connectedList+memcpyedSize+10, (*it).mcode.c_str(), mcodeSize);
							memcpy(connectedList+memcpyedSize+10+mcodeSize, (*it).custom.c_str(), customSize);
							memcpyedSize += 10+mcodeSize+customSize;
						}

					client.ws->send(connectedList, 1 + mcodeSizes + customSizes + 10*clientList.size(), uWS::OpCode::BINARY);
					stopEncoder();
					startEncoder(clbCtx);
					delete connectedList;
				}
				else
					client.ws->terminate();
				break;
			}
		}
	};
	
	std::function<void(uWS::WebSocket<uWS::SERVER>*, char*, size_t)> onClosePtr = 
	[&](uWS::WebSocket<uWS::SERVER>* ws, char* msg, size_t size) -> void {
		boost::unique_lock<boost::shared_mutex> lock(sharedMtx);

		WSType::iterator it = clientList.get<0>().find(ws);
		clientList.erase(it);

		if(clientList.size() == 0)
			clbStateCallback(STOP_ENCODING);
	};
	
	bool isSSL = false;
	if(server.initialize(port, isSSL, onOpenPtr, onMsgPtr, onClosePtr)) {
#ifdef DEBUG
		cout << "server initialized. port number is " << port << ". SSL is ";
		if(isSSL)
			cout << "enabled" << endl;
		else
			cout << "disabled" << endl;
#endif
	}
#ifdef DEBUG
	else {
		cout << "server connection problem occured!!" << endl;
		return;
	}
#endif
	
	server.run();
	cout << "program end" << endl;	
}

void clbStop(void) {
	clbStateCallback(STOP_ENCODING);
	server.stop();
}

//stop 및 initialize, encodeVideo, encodeAudio는 thread safe하게 코딩하였으므로, mutex를 신경쓸 필요 없음
void startEncoder(ClbContext mClbCtx) {
	boost::lock_guard<boost::mutex> guard(encoderMtx);
	clbCtx = mClbCtx;

	firstCount = 0;
	headerSize = 0;
	EncoderContext eCtx;
        eCtx.filePath = "test.mp4";
	if(clbCtx.isVideoEnabled) {
        	eCtx.videoCodecName = "h264_nvenc";
		switch(clbCtx.inPixFmt) {
		case PIX_FMT_R8G8B8:
			eCtx.inPixFmt = AV_PIX_FMT_RGB24;
			break;
		}
		eCtx.outPixFmt = AV_PIX_FMT_YUV420P;
		eCtx.width = clbCtx.width;
		eCtx.height = clbCtx.height;
		eCtx.framerate = clbCtx.fps;
		eCtx.keyFramerate = 128;
		eCtx.videoBitrate = 1024*512;
	}
	else
		eCtx.videoCodecName = "none";
	
	if(clbCtx.isAudioEnabled) {
		eCtx.audioCodecName = "libfdk_aac";
		switch(clbCtx.inSmpFmt) {
		case SMP_FMT_S16:
			eCtx.inSmpFmt = AV_SAMPLE_FMT_S16;
			break;
		case SMP_FMT_S32:
			eCtx.inSmpFmt = AV_SAMPLE_FMT_S32;
			break;
		}
		eCtx.outSmpFmt = AV_SAMPLE_FMT_S16;
		eCtx.samplerate = clbCtx.samplerate;
		eCtx.channels = clbCtx.channels;
		switch(eCtx.channels) {
		case 1:
			eCtx.channel_layout = AV_CH_LAYOUT_MONO;
			break;
		case 2:
			eCtx.channel_layout = AV_CH_LAYOUT_STEREO;
			break;
		}
		eCtx.audioBitrate = 1024*100;
	}
	else
		eCtx.audioCodecName = "none";

	switch(encoder.initialize(eCtx, (void*)&server, &readPacket)) {
	case SUCCESS:
		cout << "start encoder success" << endl;
		break;
	default:
		cout << "error occured" << endl;
		break;
	}
}


void stopEncoder(void) {
	boost::lock_guard<boost::mutex> guard(encoderMtx);
	encoder.stop();
}

void encodeVideo(uint8_t* data, int size) {
	if(!encoder.isStop()) {
		EncoderError err = encoder.encodeVideo(data, size);
		if(err != SUCCESS)
			printf("encode video error occured. error is %d\n");
	}
}

void encodeAudio(uint8_t* data, int size) {
	if(!encoder.isStop()) {
		EncoderError err = encoder.encodeAudio(data, size);
		if(err != SUCCESS)
			printf("encode audio error occured. error is %d\n");
	}
}
