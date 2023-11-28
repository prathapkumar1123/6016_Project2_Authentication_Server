#include "Database.h"

#include <iostream>

using namespace std;

Database::Database()
	: m_Connected(false)
	, m_Connection(nullptr)
	, m_Driver(nullptr)
	, m_ResultSet(nullptr)
	, m_Statement(nullptr) {
}

Database::~Database()
{
	if (m_Connection != nullptr)
		delete m_Connection;

	if (m_ResultSet != nullptr)
		delete m_ResultSet;

	if (m_Statement != nullptr)
		delete m_Statement;
}

void Database::ConnectToDatabase(const char* host, const char* username, const char* password, const char* schema)
{
	if (m_Connected) return;

	try
	{
		m_Driver = sql::mysql::get_mysql_driver_instance();
		m_Connection = m_Driver->connect(host, username, password);
		m_Statement = m_Connection->createStatement();

		std::string createSchemaQuery = "CREATE SCHEMA IF NOT EXISTS " + std::string(schema);
		m_Statement->execute(createSchemaQuery);

		// Set the Schema you want to use.
		m_Connection->setSchema(schema);

		CreateTables();
	}
	catch (sql::SQLException& e)
	{
		/*
		  MySQL Connector/C++ throws three different exceptions:

		  - sql::MethodNotImplementedException (derived from sql::SQLException)
		  - sql::InvalidArgumentException (derived from sql::SQLException)
		  - sql::SQLException (derived from std::runtime_error)
		*/
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
		/* what() (derived from std::runtime_error) fetches error message */
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;

		printf("Failed to connect to the Database! :( \n");
		return;
	}

	m_Connected = true;
}

void Database::Disconnect()
{
	if (!m_Connected) return;
	m_Connection->close();
	m_Connected = false;
}


sql::PreparedStatement* Database::PrepareStatement(const char* query)
{
	return m_Connection->prepareStatement(query);
}

sql::ResultSet* Database::Select(const char* query)
{
	try
	{
		// Add try catch blocks to each sql call
		m_ResultSet = m_Statement->executeQuery(query);
		return m_ResultSet;
	}
	catch (sql::SQLException& e)
	{
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

int Database::Update(const char* query)
{
	try
	{
		// Add try catch blocks to each sql call
		int count = m_Statement->executeUpdate(query);
		return count;
	}
	catch (sql::SQLException& e)
	{
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

int Database::Insert(const char* query)
{
	try
	{
		int count = m_Statement->executeUpdate(query);
		return count;
	}
	catch (sql::SQLException& e)
	{
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

void Database::CreateTables() {
	try
	{
		std::string createTableQuery = "CREATE TABLE IF NOT EXISTS web_auth ("
			"id BIGINT AUTO_INCREMENT PRIMARY KEY, "
			"email VARCHAR(255) UNIQUE NOT NULL, "
			"salt CHAR(64) NOT NULL, "
			"hashed_password CHAR(64) NOT NULL, "
			"userId BIGINT"
			")";
		m_Statement->execute(createTableQuery);

		std::string createUserTableQuery = "CREATE TABLE IF NOT EXISTS user ("
			"id BIGINT AUTO_INCREMENT PRIMARY KEY, "
			"email VARCHAR(255) UNIQUE NOT NULL, "
			"last_login TIMESTAMP, "
			"creation_date DATETIME"
			")";
		m_Statement->execute(createUserTableQuery);
	}
	catch (sql::SQLException& e)
	{
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

