#include "MySQLConnector.hpp"

MySQLConnector::MySQLConnector(MySQLContext ctx) {
	strncpy(serverIp, ctx.ip, 15);
	strncpy(serverId, ctx.id, 255);
	strncpy(serverPw, ctx.pw, 255);
	strncpy(serverSchema, ctx.schema, 255);
}

bool MySQLConnector::isSuited(const char* ip, const char* mcode, const char* mscode, bool& checkAuth, bool& checkChief, int& role) {
	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;

	driver = get_driver_instance();
	con = driver->connect(serverIp, serverId, serverPw);
	if(con != NULL) {
		con->setSchema(serverSchema);
		stmt = con->createStatement();
	}
	else
		return false;

	try {
		//		string query = "";// WHERE ip LIKE ";
		//query.append(ip);
		//table 이름은 `(~부분)로 감싸고, value는 '("부분)로 감싸야 한다.
		/*
		sql::SQLString str = "SELECT mnum FROM `member` WHERE ipaddress='";
		str.append(ip);
		str.append("' AND mcode='");
		str.append(mcode);
		str.append("'");
		res = stmt->executeQuery(str);
		*/
		sql::SQLString str = "SELECT mnum FROM `member` WHERE mcode='";
		str.append(mcode);
		str.append("'");
		res = stmt->executeQuery(str);
//		cout << res->getInt("mnum");
		//stmt에 column이 쌓이므로, res2는 2번에서 받아와야함.
		if(res->next()) {
			char mnumChar[10];
			char meetSeqChar[10];
			sprintf(mnumChar, "%d", res->getInt("mnum"));
			string decoded = Base64::decode(string(mscode));
			const char* p = strstr(decoded.c_str(), "from");
			memset(meetSeqChar,0,10*sizeof(char));
			strncpy(meetSeqChar, decoded.c_str(), p-decoded.c_str());

			str = "SELECT * FROM `participant` WHERE meet_seq='";
			str.append(meetSeqChar);
			str.append("' AND mnum='");
			str.append(mnumChar);
			str.append("'");
			res = stmt->executeQuery(str);
			if(res->next()) {
				std::istream * stream = res->getBlob("role");
				if (stream) {
					char auth[5]; // PASSWORD_LENGTH defined elsewhere; or use other functions to retrieve it
					stream->getline(auth, 5);
					int intAuth =  atoi(auth);
					role = intAuth;
					if(intAuth == 0) {
						checkAuth = true;
						checkChief = true;
					}
					else if(intAuth == 1) {
						checkAuth = true;
						checkChief = false;
					}
					else if(intAuth == 2) {
						checkAuth = false;
						checkChief = false;
					}
					delete res;
					delete stmt;
					delete con;
					return true;
				}
			}
		}	

		delete res;
		delete stmt;
		delete con;
		return false;
	}catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;
		cout << " (MySQL error code: " << e.getErrorCode() << endl;
		//cout << ", SQLState: " << e.getSQLState() << " )" << endl;
		return false;
	}
}

bool MySQLConnector::getVMInfo(const char* mscode, int& password) {
	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;

	driver = get_driver_instance();
	con = driver->connect(serverIp, serverId, serverPw);
	if(con != NULL) {
		con->setSchema(serverSchema);
		stmt = con->createStatement();
	}

	try {
		sql::SQLString str = "SELECT storagepassword FROM `meeting` WHERE mscode='";
		str.append(mscode);
		str.append("'");
		res = stmt->executeQuery(str);

		if(res->next()) {
			std::istream * stream = res->getBlob("storagepassword");
			char passwordChar[11];
			if (stream) {
				stream->getline(passwordChar, 11);
				password = atoi(passwordChar);
				return true;
			}
			else
				return false;
		}
		
		delete res;
		delete stmt;
		delete con;
		return false;
	}catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;
		cout << " (MySQL error code: " << e.getErrorCode() << endl;
		//cout << ", SQLState: " << e.getSQLState() << " )" << endl;
		return false;
	}
}

void MySQLConnector::changeAuth(const char* dst, bool authority, const char* mscode) {
	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;

	driver = get_driver_instance();
	con = driver->connect(serverIp, serverId, serverPw);
	if(con != NULL) {
		con->setSchema(serverSchema);
		stmt = con->createStatement();
	}

	try {
		//		string query = "";// WHERE ip LIKE ";
		//query.append(ip);
		//table 이름은 `(~부분)로 감싸고, value는 '("부분)로 감싸야 한다.
		char meetSeqChar[10];
		string decoded = Base64::decode(string(mscode));
		const char* p = strstr(decoded.c_str(), "from");
		memset(meetSeqChar,0,10*sizeof(char));
		strncpy(meetSeqChar, decoded.c_str(), p-decoded.c_str());

		sql::SQLString str = "SELECT mnum FROM `member` WHERE mcode='";
		str.append(dst);
		str.append("'");
		res = stmt->executeQuery(str);

		if(res->next()) {
			char mnumChar[10];
			sprintf(mnumChar, "%d", res->getInt("mnum"));
			sql::SQLString str2;
			if(authority == true)
				str2 = "UPDATE `participant` SET role='1' WHERE mnum='";
			else if(authority == false)
				str2 = "UPDATE `participant` SET role='2' WHERE mnum='";
			str2.append(mnumChar);
			str2.append("'");
			str2.append(" AND meet_seq='");
			str2.append(meetSeqChar);
			str2.append("'");
			stmt->executeUpdate(str2);
		}
		delete res;
		delete stmt;
		delete con;
	}catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;
		cout << " (MySQL error code: " << e.getErrorCode() << endl;
		//cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

void MySQLConnector::changeChief(const char* src, const char* dst, const char* mscode) {
	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;

	driver = get_driver_instance();
	con = driver->connect(serverIp, serverId, serverPw);
	if(con != NULL) {
		con->setSchema(serverSchema);
		stmt = con->createStatement();
	}

	try {
		//		string query = "";// WHERE ip LIKE ";
		//query.append(ip);
		//table 이름은 `(~부분)로 감싸고, value는 '("부분)로 감싸야 한다.

		char meetSeqChar[10];
		string decoded = Base64::decode(string(mscode));
		const char* p = strstr(decoded.c_str(), "from");
		memset(meetSeqChar,0,10*sizeof(char));
		strncpy(meetSeqChar, decoded.c_str(), p-decoded.c_str());

		sql::SQLString srcStr = "SELECT mnum FROM `member` WHERE mcode='";
		srcStr.append(src);
		srcStr.append("'");
		res = stmt->executeQuery(srcStr);

		if(res->next()) {		
			char srcChar[10];
			sprintf(srcChar, "%d", res->getInt("mnum"));

			sql::SQLString srcStr2 = "UPDATE `participant` SET role=1 WHERE mnum=";
			srcStr2.append(srcChar);
			srcStr2.append(" AND meet_seq=");
			srcStr2.append(meetSeqChar);
			cout << srcStr.c_str() << endl;
			cout << srcStr2.c_str() << endl;
			stmt->executeUpdate(srcStr2);
		}

		sql::SQLString dstStr = "SELECT mnum FROM `member` WHERE mcode='";
		dstStr.append(dst);
		dstStr.append("'");
		res = stmt->executeQuery(dstStr);
		
		if(res->next()) {
			char dstChar[10];
			sprintf(dstChar, "%d", res->getInt("mnum"));

			sql::SQLString dstStr2 = "UPDATE `participant` SET role=0 WHERE mnum=";
			dstStr2.append(dstChar);
			dstStr2.append(" AND meet_seq=");
			dstStr2.append(meetSeqChar);
			cout << dstStr.c_str() << endl;
			cout << dstStr2.c_str() << endl;
			stmt->executeUpdate(dstStr2);
		}
		delete res;
		delete stmt;
		delete con;
	}catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;
		cout << " (MySQL error code: " << e.getErrorCode() << endl;
		//cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

