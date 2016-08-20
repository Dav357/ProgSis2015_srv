/*
 * Logger.cpp
 *
 *  Created on: 20 ago 2015
 *      Author: davide
 */

#include "Logger.h"
#define BUF_LEN 120
using namespace std;

ofstream Logger::general_log;
struct timeval Logger::start_program_time;

// Costruttore: inizia il log all'apertura di un processo
Logger::Logger(string gen_log) {

  char buffer_g[BUF_LEN] = "";
  char buffer[BUF_LEN] = "";
  // Impostazione del formato date ad italiano
  setlocale(LC_ALL, "it_IT");
  // UNIX timestamp in secondi e microsecondi
  gettimeofday(&start_program_time, NULL);
  general_log.open("./Log/" + gen_log, ofstream::out);

  //Stuct time_info presa dai secondi della struct timeval
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer_g, BUF_LEN, "%A", time_info);
  strftime(buffer, BUF_LEN, " %e/%m/%Y, %H:%M:%S.", time_info);
  buffer_g[0] = toupper(buffer_g[0]);
  ANSIitoUTF(buffer_g);

  general_log << "Avvio processo: " << buffer_g << buffer << (start_program_time.tv_usec / 1000) << endl;
}

// Scrive il messaggio ricevuto nei modi specificati, default: messaggio di debug da emettere sia in console che nel log degli eventi
void Logger::writeToLog(std::string msg, int type, int scope) {

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

// Funzione di riapertura del log, per i processi figli (altrimenti continuerebbero a scrivere sul file aperto dal padre
void Logger::reopenLog(string chl_log) {

  char buffer_g[BUF_LEN] = "";
  char buffer[BUF_LEN] = "";
  // UNIX timestamp in secondi e microsecondi
  gettimeofday(&start_program_time, NULL);
  general_log.close();
  general_log.open("./Log/" + chl_log, ofstream::out);

  //Stuct time_info presa dai secondi della struct timeval
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer_g, BUF_LEN, "%A", time_info);
  strftime(buffer, BUF_LEN, " %e/%m/%Y, %H:%M:%S.", time_info);
  buffer_g[0] = toupper(buffer_g[0]);
  Logger::ANSIitoUTF(buffer_g);

  general_log << "Avvio processo: " << buffer_g << buffer << (start_program_time.tv_usec / 1000) << endl;

}

// Distruttore: chiude il log con data e ora di terminazione del processo
Logger::~Logger() {

  char buffer_g[BUF_LEN] = "";
  char buffer[BUF_LEN] = "";
  gettimeofday(&start_program_time, NULL);
  struct tm *time_info = gmtime(&(start_program_time.tv_sec));
  time_info->tm_hour += 2;
  strftime(buffer_g, BUF_LEN, "%A", time_info);
  strftime(buffer, BUF_LEN, " %e/%m/%Y, %H:%M:%S.", time_info);
  buffer_g[0] = toupper(buffer_g[0]);
  Logger::ANSIitoUTF(buffer_g);
  general_log << "Chiusura processo: " << buffer_g << buffer << (start_program_time.tv_usec / 1000) << endl;

}

void Logger::ANSIitoUTF(char* str) {
  int final_pos = strlen(str) - 1;
    if (str[final_pos] == (char)0xFFFFFFEC) {
      // UTF-8 per ì
      str[final_pos] = 0xC3;
      str[final_pos+1] = 0xAC;
      // NULL
      str[final_pos+2] = 0x00;
    }
}
