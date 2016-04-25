/*
 * Logger.h
 *
 *  Created on: 20 ago 2015
 *      Author: davide
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <fstream>
#include <string>

#define DEBUG 0x0001
#define ERROR 0x0002
#define LOG_ONLY 0x1000
#define CONSOLE_ONLY 0x2000
#define LOG_AND_CONSOLE 0x3000

class Logger {
public:
	Logger(std::string);
	static void reopen_log(std::string);
	static void write_to_log(std::string msg, int type = DEBUG, int scope = LOG_AND_CONSOLE) ;
	virtual ~Logger();

private:

	static struct timeval start_program_time;
	static std::ofstream general_log;
};

#endif /* LOGGER_H_ */
