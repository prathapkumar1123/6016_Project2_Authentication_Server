#pragma once

#include <string>
#include <mysql/jdbc.h>

class Database
{
public:
	Database();
	~Database();

	void ConnectToDatabase(const char* host, const char* username, const char* password, const char* schema);
	void Disconnect();

	sql::PreparedStatement* PrepareStatement(const char* query);

	sql::ResultSet* Select(const char* query);

	int Update(const char* query);
	int Insert(const char* query);

private:
	sql::mysql::MySQL_Driver* m_Driver;
	sql::Connection* m_Connection;
	sql::ResultSet* m_ResultSet;
	sql::Statement* m_Statement;

	bool m_Connected;

	void CreateTables();
};

