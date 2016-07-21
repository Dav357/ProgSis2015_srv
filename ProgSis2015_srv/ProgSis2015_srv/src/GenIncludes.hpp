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

// Flag per l'operazione di login
#define LOGIN 0x01
#define CREATEACC 0x02
// Dimensione buffer usati sia per la comunicazione con il client, sia internamente
#define MAX_BUF_LEN 1024
// Lunghezza comandi e risposte
#define COMM_LEN 8
// Dimensione area di memoria condivisa, dipende da lunghezza della linea e dal numero massimo di slot per gli utenti
#define MMAP_SIZE MMAP_LINE_SIZE*MMAP_MAX_USER
// Lunghezza massima linea dell'area condivisa, da cui dipende la lunghezza massima del nome utente
#define MMAP_LINE_SIZE 64
// Numero massimo di slot per gli utenti
#define MMAP_MAX_USER 128

void send_command(int s_c, const char *command);
bool add_to_usertable(std::string);
void remove_from_usertable(std::string);

#endif /* GENINCLUDES_HPP_ */
