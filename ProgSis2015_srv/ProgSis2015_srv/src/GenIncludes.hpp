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
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <cstring>
#include <chrono>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include "Logger.h"
#include "ServerSettings.h"
#include "Account.h"
#include "File.h"
#include "Folder.h"
#include "Backup.h"

// Flag per l'operazione di login
const char LOGIN = 0x01;
const char CREATEACC = 0x02;
// Lunghezza comandi e risposte
const char COMM_LEN = 8;
// Dimensione buffer usati sia per la comunicazione con il client, sia internamente
const int MAX_BUF_LEN = 1024;

void sendCommand(int s_c, const char *command);
bool addToUsertable(std::string);
void removeFromUsertable(std::string);
size_t get_max_username_len();

#endif /* GENINCLUDES_HPP_ */
