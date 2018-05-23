#include "AgentManager.hpp"

AgentManager::AgentManager() {
}

AgentManager::~AgentManager() {
	stop();
}

void AgentManager::run(int port, const char* seq, const char* password) {
	agentPort = port;
	meetSeq = seq;
	agentPassword = password;
	thread(&AgentManager::manageImpl, this).detach();
}

void AgentManager::stop() {
	stopped = true;
}

void AgentManager::manageImpl() {
	cout << "agent port is " << agentPort << ", meetSeq is " << meetSeq << ", password is " << agentPassword << endl;
	stopped = false;
	int sock;
	struct sockaddr_in server;

	while(!stopped) {
		if((sock = socket(PF_INET, SOCK_STREAM,0)) < 0) {
			cout << "Could not create agent socket" << endl;
			return;
		}
		else {
			struct timeval tv;
			tv.tv_sec = 3;
			tv.tv_usec = 0;
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
		}

		bzero((char*)&server, sizeof(server));
		server.sin_addr.s_addr = inet_addr("163.180.117.216");
		server.sin_family = AF_INET;
		server.sin_port = htons(agentPort);
	
		if(connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
			cout << "connection request..." << endl;
			close(sock);
			sleep(1);
			continue;
		}
		else {
			cout << "agent connected" << endl;
			char recvCheck[4];
			int recved = 0;
			int iResult;
			bool isTimeout = false;
			while(recved < 4) {
				iResult = recv(sock, recvCheck + recved, 4 - recved, 0);
				if(iResult < 0)
					cout << "recv error" << endl;
				else if(iResult == 0) {
					cout << "recv timeout" << endl;
					isTimeout = true;
					break;
				}
				else
					recved += iResult;
			}
			
			if(isTimeout) {
				close(sock);
				continue;
			}

			if(recvCheck[0] == 0x12 && recvCheck[1] == 0x34 && recvCheck[2] == 0x56 && recvCheck[3] == 0x78) {
				int sent = 0;
				char sentData[4] = {0x78,0x56,0x34,0x12};
				while(sent < 4) {
					iResult = send(sock, sentData + sent, 4 - sent, 0);
					if(iResult < 0)
						cout << "send error" << endl;
					else if(iResult == 0) {
						cout << "sent timeout" << endl;
						isTimeout = true;
						break;
					}
					else
						sent += iResult;
				}
				
				if(isTimeout) {
					close(sock);
					continue;
				}

                                int meetSeqLength = strlen(meetSeq);
                                int agentPasswordLength = strlen(agentPassword);

                                send(sock, (char*)(&meetSeqLength), sizeof(int), 0);
                                send(sock, meetSeq, meetSeqLength, 0);
                                send(sock, (char*)(&agentPasswordLength), sizeof(int), 0);
                                send(sock, agentPassword, agentPasswordLength, 0);
			}			
			else {
				close(sock);
				continue;
			}	
		}
	}
}
