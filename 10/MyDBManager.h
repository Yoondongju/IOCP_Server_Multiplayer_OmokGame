#pragma once
#include "mysql.h"

class MyDBManager
{
public:
	MyDBManager() = default;
	~MyDBManager() = default;

public:
    bool Connect() {
        m_mysqlConn = mysql_init(nullptr);
        if (!m_mysqlConn) return false;

        if (!mysql_real_connect(m_mysqlConn, "localhost", "root", "0928", "game_db", 3306, nullptr, 0)) {
            mysql_close(m_mysqlConn);
            m_mysqlConn = nullptr;
            return false;
        }
        return true;
    }

    void Disconnect() {
        if (m_mysqlConn) {
            mysql_close(m_mysqlConn);
            m_mysqlConn = nullptr;
        }
    }

    bool IsConnected() {
        if (!m_mysqlConn) return false;
        return mysql_ping(m_mysqlConn) == 0;
    }

    MYSQL* const Get_SqlConn() { return m_mysqlConn; }

private:
	MYSQL* m_mysqlConn = nullptr;

};

