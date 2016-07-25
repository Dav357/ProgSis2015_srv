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

const int DEBUG = 0x0001;
const int ERROR = 0x0002;
const int LOG_ONLY = 0x1000;
const int CONSOLE_ONLY = 0x2000;
const int LOG_AND_CONSOLE = 0x3000;

// Classe logger, per una gestione più semplice delle comunicazioni dal processo
class Logger {
public:
  Logger(std::string);
  static void reopenLog(std::string);
  static void writeToLog(std::string msg, int type = DEBUG, int scope = LOG_AND_CONSOLE);
  ~Logger();

private:

  static struct timeval start_program_time;
  static std::ofstream general_log;
};

#endif /* LOGGER_H_ */
