/*
 * Folder.h
 *
 *  Created on: 05 ago 2015
 *      Author: davide
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
  void insert_file(File);
  void clear_folder();
  std::vector<File> get_files() {
    return files;
  }
  bool create_table_folder();
  void get_folder_stat(int);
  void remove_files();
  bool full_backup(int);
  void send_backup(int);
  void send_single_file(int);
  bool new_file_backup(int);
  bool delete_file_backup(int);
  bool receive_file(int socket, int versione, SQLite::Database&, std::string);
  void SQL_copy_rows(SQLite::Database&, int);
};

#endif /* FOLDER_H_ */
