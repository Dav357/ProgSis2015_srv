/*
 * File.h
 *
 *  Created on: 07 ago 2015
 *      Author: davide
 */

#ifndef FILE_H_
#define FILE_H_

class File {
private:
	// Percorso cartella in cui si trova il file (REMOTO)
	Folder base_path;
	// Nome del file
	std::string filename;
	// Dimensione file (in Byte)
	size_t size;
	// Timestamp ultima modifica file
	time_t timestamp;
	// Hash del file
	std::string hash;


public:
	// Percorso locale
	std::string save_path;
	File(std::string, Folder&, size_t, time_t, char*);
	File(std::string, Folder&, size_t, time_t, std::string);
	std::string getFullPath();
	std::string getName(){return filename;};
	Folder& getBasePath(){return base_path;};
	size_t getSize(){return size;};
	time_t getTimestamp(){return timestamp;};
	std::string getHash(){return hash;};
};

bool receive_file(int, Folder&);
bool receive_file_data (int, File&);

#endif /* FILE_H_ */
