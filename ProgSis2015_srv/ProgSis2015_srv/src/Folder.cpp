/*
 * Folder.cpp
 *
 *  Created on: 05 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include "Folder.h"
using namespace std;

/* Metodi classe Folder */
Folder::Folder(string p, Account user) {

	string tmp("table_" + user.getUser() + "_" + p);

	path = p;
	table_name = tmp;
	this->user = user;
}

void Folder::clear_folder() {
	path = "";
	table_name = "";
	files.clear();
}

void Folder::insert_file(File f) {
	files.push_back(f);
}
/* Fine metodi classe Folder */

Folder select_folder(int s_c, Account& usr) {

	using namespace SQLite;
	int len;
	char buffer[MAX_BUF_LEN] = "";
	string path;

	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	if (len != 0 && len != -1) {
		buffer[len] = '\0';
		path.assign(buffer);

		// Costruzione nome tabella relativa alla cartella considerata
		Folder f(path, usr);

		// Apertura DB (sola lettura)
		Database db("database.db3");

		if (db.tableExists(f.getTableName())) {
			/* DEBUG */cout << "Trovata tabella relativa alla cartella " << path << endl;

		} else {
			/* DEBUG */cout << "Tabella relativa alla cartella " << path << " non trovata, creazione tabella" << endl;
			create_table_folder(f);
		}

		send_command(s_c, "FOLDEROK");
		return f;
	} else {
		/* DEBUG */cerr<<"Errore nella ricezione del percorso" << endl;
		Folder f;
		send_command(s_c, "FOLD_ERR");
		return f;

	}
}

bool create_table_folder(Folder& f) {
	using namespace SQLite;
	Database db("database.db3", SQLITE_OPEN_READWRITE);
	string _query("CREATE TABLE [");
	_query.append(f.getTableName());
	_query.append("] (File_ID INTEGER PRIMARY KEY AUTOINCREMENT, File_CL TEXT NOT NULL, Last_Modif INTEGER (8) NOT NULL, File_SRV TEXT NOT NULL,  Hash STRING (32), Versione_BCK INTEGER (2));");
	Statement query(db, _query);

	if (query.exec() == 0) {
		/* DEBUG */cout << "Tabella " << f.getTableName() << " creata con successo" << endl;
		return true;
	} else {
		/* DEBUG */cerr << "Errore nella creazione della tabella " << f.getTableName() << endl;
		return false;
	}

}

void get_folder_stat(int s_c, Folder& f) {

	int len, vrs;
	char buf[MAX_BUF_LEN];
	SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);

	// Selezione versione backup (numero)
	send_command(s_c, "VERS_NUM");
	/* DEBUG */cout << "In attesa del numero di versione" << endl;
	len = recv(s_c, buf, MAX_BUF_LEN, 0);
	if ((len != 0) && (len != -1)) {
		buf[len] = '\0';
		vrs = strtol(buf, NULL, 10);
		string _query("SELECT File_CL, Last_Modif, Hash FROM '" + f.getTableName() + "' WHERE Versione_BCK = ?;");
		SQLite::Statement query(db, _query);
		query.bind(1, vrs);
		// Dato il numero di backup desiderato inviare per ogni file:
		query.executeStep();
		for (;;) {
			string nome = query.getColumn("File_CL");
			time_t timestamp = query.getColumn("Last_Modif");
			string hash = query.getColumn("Hash");
			// [Percorso completo]\r\n[Ultima modifica -> 8byte][Hash -> 32char]\r\n
			int cur_file_len = nome.length() + 2/*\r\n*/+ 8 * /*Ultima modifica*/+32/*Hash*/+ 2/*\r\n*/+ 1/*\0*/;
			char cur_file[cur_file_len];
			sprintf(cur_file, "%s\r\n%016lx%s\r\n", nome.c_str(), timestamp, hash.c_str());
			/*string to_send(nome + "\r\n" + to_string(timestamp) + hash + "\r\n");*/
			send(s_c, cur_file, cur_file_len, 0);
			len = recv(s_c, buf, COMM_LEN * 2, 0);
			if (len == 8) {
				buf[COMM_LEN] = '\0';
				if (!strcmp(buf, "DATA_REC")) {
					/* DEBUG */cout << "Informazioni file inviate" << endl;
					if(query.executeStep())
						continue;
					else
						return;
				}
			} else if ((len == 0) || (len == -1)) {
				// Connessione interrotta
				/* DEBUG */cerr << "Connessione interrotta" << endl;
				close(s_c);
				return;
			}
		}

	} else {
		;
	}
	return;
}

