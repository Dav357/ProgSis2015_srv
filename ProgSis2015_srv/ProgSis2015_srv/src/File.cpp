/*
 * File.cpp
 *
 *  Created on: 07 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include <boost/filesystem.hpp>
#include "File.h"
#include <fcntl.h>
#include <sys/stat.h>
using namespace std;

/* Metodi della classe file */
File::File(string fullpath, Folder folder, size_t size, time_t timestamp) {

	string name(fullpath, folder.getPath().length() + 1, string::npos);

	base_path = folder;
	filename = name;
	this->size = size;
	this->timestamp = timestamp;
}

string File::getFullPath() {
	string temp;
	temp.assign(base_path.getPath());
	temp.append("\\");
	temp.append(filename);
	return temp;
}

/* Fine metodi della classe file */

bool receive_file(int s_c, Folder folder) {

	int len;
	string filename;
	char buffer[MAX_BUF_LEN] = "";
	char *file_info;
	char _size_c[12 + 1] = "", hash[32 + 1] = "", _timestamp_c[16 + 1] = "";
	size_t size;
	time_t timestamp;

	// Ricezione: Nome completo file\r\n | Dimensione file (8 Byte) | Hash del file (16 Byte) | Timestamp (8 Byte)
	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	buffer[len] = '\0';
	///* DEBUG */cout << "Stringa ricevuta: " << buffer << endl << "Lunghezza: " << len << endl;
	/* Estrazione nome, dimensione, hash, timestamp */
	filename = strtok(buffer, "\r\n");
	file_info = strtok(NULL, "\r\n");
	///* DEBUG */cout << "Nome file: " << filename << endl;
	///* DEBUG */cout << "Stringa rimanente: " << file_info << endl;
	// Lettura dimensione
	strncpy(_size_c, file_info, 12);
	_size_c[12] = '\0';
	size = strtol(_size_c, NULL, 16);
	///* DEBUG */cout << "Dimensione file: " << size << endl;
	// Lettura Hash ricevuto
	strncpy(hash, file_info + 12, 32);
	hash[32] = '\0';
	///* DEBUG */cout << "Hash file: " << hash << endl;
	// Lettura timestamp ricevuto
	strncpy(_timestamp_c, file_info + 44, 16);
	_timestamp_c[16] = '\0';
	timestamp = strtoul(_timestamp_c, NULL, 16);
	///* DEBUG */cout << "Timestamp: " << timestamp << endl;

	File file(filename, folder, size, timestamp);
	///* DEBUG */cout << "Nome file: " << file.getName() << endl;
	///* DEBUG */cout << "nella cartella " << file.getBasePath().getPath() << endl;
	///* DEBUG */cout << "di dimensione " << file.getSize() << " Byte" << endl;
	///* DEBUG */cout << "Percorso completo file: " << file.getFullPath() << endl;
	///* DEBUG */timestamp = file.getTimestamp();
	///* DEBUG */struct tm *time_info = gmtime(&timestamp);
	///* DEBUG */time_info->tm_hour+=2;
	///* DEBUG */strftime(buffer, MAX_BUF_LEN, "%A %e %B %Y, %H:%M:%S", time_info);
	///* DEBUG */cout << "ultima modifica: " << buffer << endl;

	// Invio conferma ricezione dati
	send_command(s_c, "INFO_OK_");

	// Ricezione contenuto del file
	receive_file_data(file);
	send_command(s_c, "DATA_OK_");

	return false;
}

bool receive_file_data(File file_info) {

	int fpoint;
	time_t t = time(NULL);
	char buffer[MAX_BUF_LEN];
	struct tm *time_info = gmtime(&t);
	strftime(buffer, MAX_BUF_LEN, "%Y-%m-%d", time_info);

	// Costruzione nome nuovo file
	// Creazione cartella
	string path_to_file("./ReceivedFiles/");
	path_to_file.append(file_info.getBasePath().getUser());
	path_to_file.append("_");
	path_to_file.append(file_info.getBasePath().getPath());
	path_to_file.append("_");
	path_to_file.append(buffer);
	boost::filesystem::create_directories(path_to_file);
	path_to_file.append("/");
	///* DEBUG */cout << path_to_file << endl;
	// Costruzione nome file
	string fullpath(path_to_file);
	fullpath.append(file_info.getName());

	//Apertura file: in sola scrittura, crea se non esistente; permessi: -rwxr-xr-x
	if ((fpoint = open(fullpath.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO)) < 0) {
		///* DEBUG */cout << "Errore nella creazione del file " << fullpath << endl;
		return false;
	} else {
		///* DEBUG */cout << "File " << fullpath << " creato correttamente" << endl;
		//Loop di lettura bytes inviati dal client
	}
	return true;
}
