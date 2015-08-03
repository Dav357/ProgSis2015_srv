/*
 * LoginServ.hpp
 *
 *  Created on: 03 ago 2015
 *      Author: davide
 */
#ifndef LOGINSERV_HPP_
#define LOGINSERV_HPP_


#define LOGIN 0x00000001
#define CREATEACC 0x00000002
#define MAX_BUF_LEN 1024
#define COMM_LEN 8

bool login(int);
bool account_manag(std::string usr, std::string shapass, int flag);


#endif /* LOGINSERV_HPP_ */
