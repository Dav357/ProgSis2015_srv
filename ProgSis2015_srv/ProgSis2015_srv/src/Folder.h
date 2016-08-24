/*
 * Folder.h
 *
 *  Created on: 05 ago 2015
 *      Author: Davide Locatelli
 */

#ifndef FOLDER_H_
#define FOLDER_H_

class File;

// Classe per la gestione delle cartelle
class Folder {
private:
  std::string path;
  std::string table_name;
  Account user;
  std::vector<File> files;

public:
  Folder() {
  }
  Folder(std::string, Account&);
  bool isSelected() {
    return (!path.empty());
  }
  std::string getPath() {
    return path;
  }
  std::string getTableName() {
    return table_name;
  }
  std::string getUser() {
    return user.getUser();
  }
  void insertFile(File);
  void clearFolder();
  std::vector<File> getFiles() {
    return files;
  }
  bool createTableFolder();
  void getFolderStat(int);
  void removeFiles();
  bool fullBackup(int);
  void sendBackup(int);
  void sendSingleFile(int);
  bool newFileBackup(int);
  bool deleteFileBackup(int);
  bool receiveFile(int socket, int versione, SQLite::Database&, std::string);
  void SQLCopyRows(SQLite::Database&, int);
};

#endif /* FOLDER_H_ */
