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
Folder::Folder(string p, string user) {

	string tmp = "table_";
	tmp.append(user);
	tmp.append("_");
	tmp.append(p);

	path = p;
	table_name = tmp;
	this->user = user;
}

void Folder::clear_folder() {
	path = "";
	table_name = "";
}
/* Fine metodi classe Folder */

Folder select_folder(int s_c, string usr) {

	using namespace SQLite;
	int len;
	char buffer[MAX_BUF_LEN] = "";
	string path;

	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	if (len != 0 && len != -1) {
		buffer[len] = '\0';
		path.assign(buffer);

		// Costruzuone nome tabella relativa alla cartella considerata
		Folder f(path, usr);

		// Apertura DB (sola lettura)
		Database db("database.db3");

		if (db.tableExists(f.getTableName())) {
			///* DEBUG */cout << "Trovata tabella relativa alla cartella " << path << endl;

		} else {
			///* DEBUG */cout << "Tabella relativa alla cartella " << path << " non trovata, creazione tabella" << endl;
			create_table_folder(f);
		}

		send_command(s_c, "FOLDEROK");
		return f;
	} else {
		///* DEBUG */cout<<"Errore nella ricezione del persorso" << endl;
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
	_query.append("] (File_ID INTEGER PRIMARY KEY AUTOINCREMENT, File_CL TEXT NOT NULL, Last_Modif INTEGER (8) NOT NULL,File_SRV TEXT  NOT NULL,  Hash STRING (32));");
	Statement query(db, _query);

	if (query.exec() == 0) {
		///* DEBUG */cout << "Tabella " << f.getTableName() << " creata con successo" << endl;
		return true;
	} else {
		///* DEBUG */cout << "Errore nella creazione della tabella " << f.getTableName() << endl;
		return false;
	}

}

