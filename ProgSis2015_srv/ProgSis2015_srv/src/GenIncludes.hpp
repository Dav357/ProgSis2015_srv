/*
 * GenIncludes.hpp
 *
 *  Created on: 03 ago 2015
 *      Author: davide
 */

#ifndef GENINCLUDES_HPP_
#define GENINCLUDES_HPP_

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <string.h>
#include <sys/socket.h>

#include "LoginServ.hpp"
#include "Folder.h"
#include "File.h"
#include "SQLiteCpp/SQLiteCpp.h"

void send_command(int s_c, const char *command);


#endif /* GENINCLUDES_HPP_ */
