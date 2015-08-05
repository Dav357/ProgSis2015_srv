/*
 * LoginServ.hpp
 *
 *  Created on: 03 ago 2015
 *      Author: davide
 */
#ifndef LOGINSERV_HPP_
#define LOGINSERV_HPP_

#include "SQLiteCpp/SQLiteCpp.h"

#define LOGIN 0x00000001
#define CREATEACC 0x00000002
#define MAX_BUF_LEN 1024
#define COMM_LEN 8

class Account{
	std::string usr;
	std::string shapass;
	bool complete;

public:
	Account();
	Account(std::string, std::string);
	void assign(std::string, std::string);
	void assign_username(std::string);
	void assign_username(char*);
	void assign_password(std::string);
	void assign_password(char*);
	/* Getter */
	std::string getUser(){return usr;};
	bool is_complete(){return complete;};
	/**/
	bool bind_to_query(SQLite::Statement&);
	void clear();
};

Account login(int);
bool account_manag(Account& ac, int flag);

#endif /* LOGINSERV_HPP_ */
