/*
 * LoginServ.hpp
 *
 *  Created on: 03 ago 2015
 *      Author: davide
 */
#ifndef ACCOUNT_H_
#define ACCOUNT_H_

#include "SQLiteCpp/SQLiteCpp.h"

class Folder;

// Classe per la gestione dell'account
class Account {
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
  std::string getUser() {
    return usr;
  }
  bool is_complete() {
    return complete;
  }
  char account_manag(int flag);
  Folder select_folder(int);
  void clear();
};

Account login(int, char*, int rec = 0);

#endif /* ACCOUNT_H_ */
