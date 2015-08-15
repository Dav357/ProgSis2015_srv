/*
 * Folder.h
 *
 *  Created on: 05 ago 2015
 *      Author: davide
 */

#ifndef FOLDER_H_
#define FOLDER_H_

#include <vector>

class File;

class Folder {
private:
	std::string path;
	std::string table_name;
	Account user;
	std::vector<File> files;

public:
	Folder(){};
	Folder(std::string, Account);
	std::string getPath(){return path;}
	std::string getTableName(){return table_name;}
	std::string getUser(){return user.getUser();}
	void insert_file(File);
	void clear_folder();
	std::vector<File> get_files(){return files;};
};

Folder select_folder(int, Account& ac);
bool create_table_folder(Folder&);
void get_folder_stat(int, Folder&);

#endif /* FOLDER_H_ */
