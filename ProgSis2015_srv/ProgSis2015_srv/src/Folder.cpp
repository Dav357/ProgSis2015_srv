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
Folder::Folder(string p, string usr) {

	string tmp = "table_";
	tmp.append(usr);
	tmp.append("_");
	tmp.append(p);

	path = p;
	table_name = tmp;
}

void Folder::clear_folder(){

}
/* Fine metodi classe Folder */

Folder select_folder(int s_c, string usr) {

	using namespace SQLite;
	int len;
	char buffer[MAX_BUF_LEN] = "";
	string path;
	string table_name = "table_";

	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	buffer[len] = '\0';
	path.assign(buffer);

	// Costruzuone nome tabella relativa alla cartella considerata
	Folder f(path, usr);

	// Apertura DB (sola lettura)
	Database db("database.db3");

	if(db.tableExists(f.getTableName())){
		/* DEBUG */cout << "Trovata tabella relativa alla cartella " << path << endl;
	} else {
		/* DEBUG */cout << "Tabella relativa alla cartella " << path << " non trovata" << endl;
	}

	return f;
}
