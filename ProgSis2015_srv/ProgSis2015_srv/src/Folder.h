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

public:
	Folder(){};
	Folder(std::string, std::string);
	std::string getTableName(){return table_name;}
	void clear_folder();
};

Folder select_folder(int, std::string);

#endif /* FOLDER_H_ */
