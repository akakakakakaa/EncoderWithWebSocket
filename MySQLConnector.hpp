#pragma once
#include <iostream>
#include <fstream>
#include <string>
//MySQL 관련 헤더파일
#include <mysql_connection.h>
//#include <cppconn\connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include "Base64.hpp"
using namespace std;

struct MySQLContext {
	const char *ip;
	const char *id;
	const char *pw;
	const char *schema;
};

class MySQLConnector {
public:
	MySQLConnector(MySQLContext ctx);
	bool isSuited(const char* ip, const char* mcode, const char* mscode, bool& checkAuth, bool& checkChief, int& role);
	bool getVMInfo(const char* mscode, int& password);
	void changeAuth(const char* dst, bool authority, const char* mscode);
	void changeChief(const char* src, const char* dst, const char* mscode);

private:
	char serverIp[255];
	char serverId[255];
	char serverPw[255];
	char serverSchema[255];
};
