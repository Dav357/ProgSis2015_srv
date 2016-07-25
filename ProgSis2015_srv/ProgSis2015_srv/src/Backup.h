/*
 * Backup.h
 *
 *  Created on: 10 ago 2015
 *      Author: davide
 */

#ifndef BACKUP_H_
#define BACKUP_H_

class Backup {
private:
  int vers;
  bool complete;
  Folder &folder;
public:
  Backup(int, Folder&);
  virtual ~Backup();
  void completed();
  bool isComplete() {
    return complete;
  }
};

void dbMaintenance(std::string);
bool fullBackup(int, Folder&);

#endif /* BACKUP_H_ */
