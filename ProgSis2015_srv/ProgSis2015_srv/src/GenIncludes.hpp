/*
 * GenIncludes.hpp
 *
 *  Created on: 03 ago 2015
 *      Author: davide
 */

#ifndef GENINCLUDES_HPP_
#define GENINCLUDES_HPP_

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <string.h>
#include <chrono>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include "Logger.h"
#include "Account.h"
#include "File.h"
#include "Folder.h"
#include "Backup.h"
#include "SQLiteCpp/SQLiteCpp.h"

#define LOGIN 0x01
#define CREATEACC 0x02
#define MAX_BUF_LEN 1024
#define SHORT_BUF_LEN 128
#define COMM_LEN 8

void send_command(int s_c, const char *command);
bool add_to_usertable(std::string);
void remove_from_usertable(std::string);

#endif /* GENINCLUDES_HPP_ */
