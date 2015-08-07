/*
 * Folder.h
 *
 *  Created on: 05 ago 2015
 *      Author: davide
 */

#ifndef FOLDER_H_
#define FOLDER_H_

class Folder {
private:
	std::string path;
	std::string table_name;
	std::string user;

public:
	Folder(){};
	Folder(std::string, std::string);
	std::string getPath(){return path;}
	std::string getTableName(){return table_name;}
	std::string getUser(){return user;}
	void clear_folder();
};

Folder select_folder(int, std::string);
bool create_table_folder(Folder);

#endif /* FOLDER_H_ */
