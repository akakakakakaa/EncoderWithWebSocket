#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

class AgentManager {
public:
	AgentManager();
	~AgentManager();
	void run(int port, const char* seq, const char* password);
	void stop();
	void manageImpl();	

private:
	int agentPort;
	bool stopped;
	const char* meetSeq;
	const char* agentPassword;
};
