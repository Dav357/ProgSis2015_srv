/*
 * File.cpp
 *
 *  Created on: 07 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include "File.h"
#include <fcntl.h>
#include <sys/stat.h>
using namespace std;

/* Metodi della classe file */
File::File(string fullpath, Folder& folder, size_t size, time_t timestamp, char *h) :
		base_path(folder) {

	string name(fullpath, folder.getPath().length() + 1, string::npos);

	//base_path = folder;
	filename = name;
	this->size = size;
	this->timestamp = timestamp;
	hash = h;
	complete = false;
}

File::File(string fullpath, Folder& folder, size_t size, time_t timestamp, string h) :
		base_path(folder) {

	string name(fullpath, folder.getPath().length() + 1, string::npos);

	//base_path = folder;
	filename = name;
	this->size = size;
	this->timestamp = timestamp;
	hash = h;
	complete = false;
}

string File::getFullPath() {
	string temp(base_path.getPath() + "\\" + filename);
	return temp;
}
/* Distruttore */
File::~File() {
	// Se la ricezione non è completa -> CANCELLAZIONE file.
	if (!complete)
		remove(save_path.c_str());
}

/* Fine metodi della classe file */
bool receive_file(int s_c, Folder& folder, int vrs, SQLite::Database& db) {

	int len;
	string filename;
	char buffer[MAX_BUF_LEN] = "";
	char *file_info;
	char _size_c[12 + 1] = "", hash[32 + 1] = "", _timestamp_c[16 + 1] = "";
	size_t size;
	time_t timestamp;

	// Ricezione: Nome completo file\r\n | Dimensione file (8 Byte) | Hash del file (16 Byte) | Timestamp (8 Byte)
	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	if (len == 0 || len == -1) {
		return false;
	}
	buffer[len] = '\0';
	/* Estrazione nome, dimensione, hash, timestamp */
	filename = strtok(buffer, "\r\n");
	file_info = strtok(NULL, "\r\n");
	// Lettura dimensione
	strncpy(_size_c, file_info, 12);
	_size_c[12] = '\0';
	size = strtol(_size_c, NULL, 16);
	// Lettura Hash ricevuto
	strncpy(hash, file_info + 12, 32);
	hash[32] = '\0';
	// Lettura timestamp ricevuto
	strncpy(_timestamp_c, file_info + 44, 16);
	_timestamp_c[16] = '\0';
	timestamp = strtoul(_timestamp_c, NULL, 16);

	File file(filename, folder, size, timestamp, hash);
	///* DEBUG */cout << "Nome file: " << file.getName() << endl;
	///* DEBUG */cout << "nella cartella " << file.getBasePath().getPath() << endl;
	///* DEBUG */cout << "di dimensione " << file.getSize() << " Byte" << endl;
	///* DEBUG */cout << "Percorso completo file: " << file.getFullPath() << endl;
	///* DEBUG */timestamp = file.getTimestamp();
	///* DEBUG */struct tm *time_info = gmtime(&timestamp);
	///* DEBUG */time_info->tm_hour+=2;
	///* DEBUG */strftime(buffer, MAX_BUF_LEN, "%A %e %B %Y, %H:%M:%S", time_info);
	///* DEBUG */cout << "ultima modifica: " << buffer << endl;

	// - Elimina informazioni relative al file dalla versione corrente del backup, se presenti
	// SQL: DELETE FROM '[nome_table]' WHERE Versione_BCK=[vrs] AND Hash=file.getHash()
	string _query("DELETE FROM '" + folder.getTableName() + "' WHERE Versione_BCK=" + to_string(vrs) + " AND File_CL='" + file.getName() + "';");
	SQLite::Statement query(db, _query);
	if (query.exec() == 0) {
		// NON È UN AGGIORNAMENTO, MA UN SEMPLICE INSERIMENTO NON LO CONSIDERO ERRORE
		/* DEBUG */cout << "Il file non è un aggiornamento, ma un file nuovo" << endl;
	} else {
		/* DEBUG */cout << "Il file è un aggiornamento" << endl;
	}

	// Invio conferma ricezione dati
	send_command(s_c, "INFO_OK_");

	// Ricezione contenuto del file
	try {
		if (receive_file_data(s_c, file)) {
			/* Inserimento file nel database */;
			string _query("INSERT INTO [");
			_query.append(folder.getTableName());
			_query.append("] (File_CL, Last_Modif, File_SRV, Hash, Versione_BCK) VALUES (?, ?, ?, ?, ?)");
			SQLite::Statement query(db, _query);
			query.bind(1, file.getName());
			query.bind(2, (long long int) file.getTimestamp());
			query.bind(3, file.save_path);
			query.bind(4, file.getHash());
			query.bind(5, vrs);
			if (query.exec() == 1) {
				/* DEBUG */cout << "Entry relativa al file inserita con successo" << endl;
				file.completed();
				folder.insert_file(file);
				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
	} catch (ios_base::failure &e) {
		/* DEBUG */cerr << "Errore nella scrittura del file: " << e.what() << endl;
		return false;
	}
}

bool receive_file_data(int s_c, File& file_info) {

	int fpoint;
	time_t t = time(NULL);
	char buffer[MAX_BUF_LEN];

	// Costruzione nome nuovo file
	string path_to_file("./ReceivedFiles/" + file_info.getBasePath().getUser() + "_" + file_info.getBasePath().getPath() + "_" + to_string(t));
	// Creazione cartella permessi: -rwxrw-r-- (764)
	mkdir(path_to_file.c_str(), S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH);
	path_to_file.append("/");

	// Costruzione nome file
	string fullpath(path_to_file + file_info.getName());
	file_info.save_path = fullpath;

	//Apertura file: in sola scrittura, crea se non esistente; permessi: -rwxrw-r-- (764)
	if ((fpoint = open(fullpath.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
		/* DEBUG */cerr << "Errore nella creazione del file " << fullpath << ", file già esistente" << endl;
		return false;
	} else {
		/* DEBUG */cout << "File " << fullpath << " creato correttamente" << endl;
		/* DEBUG */cout << "Ricezione file in corso" << endl;
		//Loop di lettura bytes inviati dal client
		int rbyte = 0;
		size_t received = 0;
		for (rbyte = recv(s_c, buffer, MAX_BUF_LEN, 0); ((rbyte != 0) && (rbyte != -1)); rbyte = recv(s_c, buffer, MAX_BUF_LEN, 0)) {
			write(fpoint, buffer, rbyte);
			received += rbyte;
			if (received == file_info.getSize()) {
				close(fpoint);
				return true;
			}
		}
		/* DEBUG */cerr << "Errore nella ricezione del file" << endl;
		throw ios_base::failure("ricezione parziale, cancellazione");
	}
}

bool new_file_backup(int s_c, Folder& f) {

	int vrs;
	// Un nuovo file deve essere inserito nel backup corrente:
	// - Si trova la versione di backup corrente
	SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
	string _query("SELECT MAX(Versione_BCK) FROM '" + f.getTableName() + "';");
	SQLite::Statement query(db, _query);
	if (query.executeStep()) {
		// Un risultato = corretto, si aggiunge all'ultimo backup
		vrs = query.getColumn(0).getInt() + 1;
	} else {
		// Non esiste un backup completo corrente -> Errore
		/* DEBUG */cerr << "Si sta tentando di aggiungere un file senza avere un backup completo" << endl;
		send_command(s_c, "MISS_BCK");
		return false;
	}
	query.reset();
	// - Si apre un backup e una transazione sul DB
	Backup bck(vrs, f);
	SQLite::Transaction trs(db);
	// - Si copiano le voci relative all'ultima versione in una nuova versione con gli stessi file
	SQL_copy_rows(db, f, (vrs - 1));
	// - Si riceve il file [receive_file]
	for (int tent = 1; tent < 6;) {
		if (receive_file(s_c, f, vrs, db)) {
			// - Se il file e le informazioni sul DB sono state processate correttamente
			// - Commit DB e backup completato
			send_command(s_c, "DATA_OK_");
			bck.completed();
			trs.commit();
			/* DEBUG */cout << "Ricezione file completata" << endl;
			return true;
		} else {
			/* DEBUG */cout << "Errore nella ricezione: tentativo " + to_string(tent) << endl;
			if (errno != EEXIST) {
				send_command(s_c, "SNDAGAIN");
				tent++;
			} else {
				/* DEBUG */cerr << "File già esistente, non verrà sovrascritto" << endl;
				send_command(s_c, "DATA_OK_");
				bck.completed();
				trs.commit();
				return true;
			}
		}
	}
	return false;
}

void SQL_copy_rows(SQLite::Database& db, Folder& f, int vrs) {
//	CREATE TABLE 'temporary_table' AS SELECT * FROM '[nome_table]' WHERE Versione_BCK= ?;
	string _query = "CREATE TABLE 'temporary_table' AS SELECT * FROM '" + f.getTableName() + "' WHERE Versione_BCK= ?;";
	SQLite::Statement query_1(db, _query);
	query_1.bind(1, vrs);
	query_1.exec();
//	UPDATE 'temporary_table' SET Versione_BCK=(Versione_BCK+1);
	_query = "UPDATE 'temporary_table' SET Versione_BCK=(Versione_BCK+1);";
	SQLite::Statement query_2(db, _query);
	query_2.exec();
//	UPDATE 'temporary_table' SET File_ID=NULL;
	_query = "UPDATE 'temporary_table' SET File_ID=NULL;";
	SQLite::Statement query_3(db, _query);
	query_3.exec();
//	INSERT INTO '[nome_table]' SELECT * FROM 'temporary_table';
	_query = "INSERT INTO '" + f.getTableName() + "' SELECT * FROM 'temporary_table';";
	SQLite::Statement query_4(db, _query);
	query_4.exec();
//	DROP TABLE 'temporary_table';
	_query = "DROP TABLE 'temporary_table';";
	SQLite::Statement query_5(db, _query);
	query_5.exec();
}
