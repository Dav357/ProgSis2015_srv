/*
 * Logger.cpp
 *
 *  Created on: 20 ago 2015
 *      Author: davide
 */

#include "Logger.h"
#include <sys/time.h>
#define BUF_LEN 120
using namespace std;

ofstream Logger::general_log;
struct timeval Logger::start_program_time;

// Costruttore: inizia il log all'apertura di un processo
Logger::Logger(string gen_log) {

  char buffer[BUF_LEN];
  // UNIX timestamp in secondi e microsecondi
  gettimeofday(&start_program_time, NULL);
  Logger::general_log.open("./Log/" + gen_log, ofstream::out);

  //Stuct time_info presa dai secondi della struct timeval
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer, BUF_LEN, "%A %e/%m/%Y, %H:%M:%S.", time_info);

  general_log << "Avvio processo: " << buffer << (start_program_time.tv_usec / 1000) << endl;

}

// Scrive il messaggio ricevuto nei modi specificati, default: messaggio di debug da emettere sia in console che nel log degli eventi
void Logger::write_to_log(std::string msg, int type, int scope) {

  string pref;
  ostream *target;
  char buffer[BUF_LEN];
  struct timeval now;
  gettimeofday(&now, NULL);

  switch (type) {
    case DEBUG:
      pref.assign(" - DEBUG - ");
      target = &cout;
    break;
    case ERROR:
      pref.assign(" X ERROR X ");
      target = &cerr;
    break;
  }

  struct tm *time_info = gmtime(&(now.tv_sec));
  time_info->tm_hour += 2;

  switch (scope) {
    case LOG_AND_CONSOLE:
    case LOG_ONLY:
      strftime(buffer, BUF_LEN, "%H:%M:%S.", time_info);
      general_log << "[" << buffer;
      sprintf(buffer, "%03ld", now.tv_usec / 1000);
      general_log << buffer << "]" << pref << msg << endl;
      if (scope == LOG_ONLY) break;
      //no break
    case CONSOLE_ONLY:
      (type == DEBUG) ? ((*target) << ("- ")) : ((*target) << ("❌ "));
      (*target) << msg << endl;
  }
}

// Distruttore: chiude il log con data e ora di terminazione del processo
Logger::~Logger() {

  char buffer[BUF_LEN];
  gettimeofday(&start_program_time, NULL);
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer, BUF_LEN, "%A %e/%m/%Y, %H:%M:%S.", time_info);
  general_log << "Chiusura processo: " << buffer << (start_program_time.tv_usec / 1000) << endl;

}

// Funzione di riapertura del log, per i processi figli (altrimenti continuerebbero a scrivere sul file aperto dal padre
void Logger::reopen_log(string chl_log) {

  char buffer[BUF_LEN];
  // UNIX timestamp in secondi e microsecondi
  gettimeofday(&start_program_time, NULL);
  Logger::general_log.close();
  Logger::general_log.open("./Log/" + chl_log, ofstream::out);

  //Stuct time_info presa dai secondi della struct timeval
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer, BUF_LEN, "%A %e/%m/%Y, %H:%M:%S.", time_info);

  general_log << "Avvio processo: " << buffer << (start_program_time.tv_usec / 1000) << endl;

}
