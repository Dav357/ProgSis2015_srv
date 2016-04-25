/*
 * File.h
 *
 *  Created on: 07 ago 2015
 *      Author: davide
 */
#ifndef FILE_H_
#define FILE_H_

#include "Folder.h"

class File {

private:
	// Riferimento all'oggeto che contiene informazioni riguardo alla cartella in cui si trova il file (REMOTO)
	Folder& base_path;
	// Nome del file
	std::string filename;
	// Dimensione file (in Byte)
	size_t size;
	// Timestamp ultima modifica file
	time_t timestamp;
	// Hash del file
	std::string hash;
	// FIle completo
	bool complete;
	// Percorso locale
	std::string save_path;
public:
	// Metodi
	File(std::string, Folder&, size_t, time_t, std::string);
	/* Costruttore per invio file */
	File(std::string, Folder&, std::string, std::string);
	/* -------------------------- */
	virtual ~File();
	std::string getFullPath();
	std::string getName(){return filename;};
	std::string getSavePath() {return save_path;};
	Folder& getBasePath(){return base_path;};
	size_t getSize(){return size;};
	time_t getTimestamp(){return timestamp;};
	std::string getHash(){return hash;};
	void completed();
	bool receive_file_data (int socket, std::string);
	void send_file_data (int socket);
	bool hash_check (int fpointer);
};



#endif /* FILE_H_ */
