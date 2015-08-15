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
Backup::Backup(int v, Folder& f): vers(v), complete(false), folder(f) {
}
Backup::~Backup(){
	if (!complete){
		// Cancella tutti i file della cartella
		for(File f : folder.get_files()){
			remove(f.save_path.c_str());
		}
		folder.clear_folder();
	}
}
void Backup::completed(){
	complete = true;
}

/* Fine metodi classe backup */

/* Backup completo */
bool full_backup(int s_c, Folder& f) {

	int vrs, len;
	char comm[2 * COMM_LEN];

	// Creazione nuova versione backup nel DB
	// 		// Lettura versione attuale
	SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
	string _query("SELECT MAX(Versione_BCK) FROM '" + f.getTableName() + "';");
	SQLite::Statement query(db, _query);
	// Esecuzione query -> una sola riga di risultato (SELECT MAX)

	if (query.executeStep())
		// Se c'è un risultato = esiste già un backup della cartella
		vrs = query.getColumn(0).getInt() + 1;
	else
		// Altrimenti: prima versione del backup
		vrs = 1;
	// Ricevere tutti  i file che il client mi manda, inserendoli in ordine di arrivo nel DB, con la versione corretta

	Backup backup(vrs, f);
	SQLite::Transaction trs(db);

	// Attesa comando: 	- se REC_FILE -> nuovo file in arrivo
	//					- se SYNC_END -> fine backup
	for (len = recv(s_c, comm, 2 * COMM_LEN, 0); len == COMM_LEN; len = recv(s_c, comm, 2 * COMM_LEN, 0)) {
		comm[len] = '\0';
		if (!strcmp(comm, "REC_FILE")) {
			// Ricevi file
			if (receive_file(s_c, f, vrs, db)) {
				// File ricevuto correttamente
				send_command(s_c, "DATA_OK_");
				continue;
			} else {
				// Errore nella ricezione del file, richiesta nuova spedizione
				send_command(s_c, "SNDAGAIN");
				continue;
			}
		} else if (!strcmp(comm, "SYNC_END")) {
			// Fine positiva backup -> commit dell'intera transazione
			trs.commit();
			backup.completed();
			break;
		} else {
			// Comando sconosciuto
			break;
		}
	}
	if (trs.isCommited()){
		// Se la transazione ha avuto commit
		// Controllo se i file appena inseriti erano già presenti, in tal caso cancello quello nuovo e sposto il percorso nel DB
		return true;
	} else {
		// Se la transazione non ha avuto commit
		// al return avrò già rollback, devo solo cancellare i file
		return false;
	}
}
