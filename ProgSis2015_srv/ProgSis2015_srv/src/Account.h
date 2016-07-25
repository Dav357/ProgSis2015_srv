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
  void assignUsername(std::string);
  void assignUsername(char*);
  void assignPassword(std::string);
  void assignPassword(char*);
  std::string getUser() {
    return usr;
  }
  bool isComplete() {
    return complete;
  }
  char accountManag(int flag);
  Folder selectFolder(int);
  void clear();
};

Account login(int, char*, int rec = 0);

void test_func();

#endif /* ACCOUNT_H_ */
