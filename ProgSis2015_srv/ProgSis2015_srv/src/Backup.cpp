/*
 * Backup.cpp
 *
 *  Created on: 10 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include "Backup.h"
using namespace std;

/* Metodi classe backup */
Backup::Backup(int v, Folder& f) :
		vers(v), complete(false), folder(f) {
}

Backup::~Backup() {
	if (!complete) {
		// Se il backup non è completo:
		// Cancella tutti i file della cartella
		for (File f : folder.get_files()) {
			remove(f.save_path.c_str());
		}
		folder.clear_folder();
	} else {
		// Altrimenti ripulisci solo il vettore dei file
		folder.remove_files();
	}
}

void Backup::completed() {
	complete = true;
}

/* Fine metodi classe backup */


//TODO Finire implementazione funzione di servizio pulizia database

void db_maintenance(string table_name) {

	try {
		SQLite::Database db("database.db3");

		// Conto del numero di versioni diverse di backup nel DB
		int count = db.execAndGet("SELECT COUNT(DISTINCT Versione_BCK) FROM '" + table_name + "';");
		// Se sono presenti più di 5 versioni
		if (count > 5) {
			// Si elimina quella più vecchia, cancellandone i file
			// Una transazione per file?
			SQLite::Transaction trs(db);
			trs.commit();
			// Si decrementa il numero di versione per le voci ancora presenti
		}
		return;
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		///* DEBUG */cerr << "- Errore DB: " << e.what() << endl;
	}
}
