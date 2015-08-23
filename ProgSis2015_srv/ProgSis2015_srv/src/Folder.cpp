/*
 * Folder.cpp
 *
 *  Created on: 05 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include "Folder.h"
using namespace std;

Folder::Folder(string p, Account& user) :
		user(user) {

	string tmp("table_" + user.getUser() + "_" + p);
	path = p;
	table_name = tmp;
}

void Folder::clear_folder() {
	path = "";
	table_name = "";
	files.clear();
}

void Folder::insert_file(File f) {
	files.push_back(f);
}

bool Folder::create_table_folder() {

	SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
	string _query(
			"CREATE TABLE [" + table_name
					+ "] (File_ID INTEGER PRIMARY KEY AUTOINCREMENT, File_CL TEXT NOT NULL, Last_Modif INTEGER (8) NOT NULL, File_SRV TEXT NOT NULL,  Hash STRING (32), Versione_BCK INTEGER (2));");
	SQLite::Statement query(db, _query);

	if (query.exec() == 0) {
		Logger::write_to_log("Tabella " + table_name + " creata con successo");
		///* DEBUG */cout << "- Tabella " << table_name << " creata con successo" << endl;
		return true;
	} else {
		Logger::write_to_log("Errore nella creazione della tabella " + table_name, ERROR);
		///* DEBUG */cerr << "- Errore nella creazione della tabella " << table_name << endl;
		return false;
	}
}

void Folder::get_folder_stat(int s_c) {

	if (!isSelected()) {
		throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
	}

	int len, vrs;
	char buf[MAX_BUF_LEN];
	try {
		SQLite::Database db("database.db3", SQLITE_OPEN_READONLY);
		// Selezione versione backup (numero)
		send_command(s_c, "VERS_NUM");
		Logger::write_to_log("In attesa del numero di versione");
		///* DEBUG */cout << "- In attesa del numero di versione" << endl;
		len = recv(s_c, buf, MAX_BUF_LEN, 0);
		if ((len == 0) || (len == -1)) {
			// Ricevuta stringa vuota: connessione persa
			throw runtime_error("connessione persa");
		}
		buf[len] = '\0';
		vrs = strtol(buf, NULL, 10);
		Logger::write_to_log("Versione richiesta: " + to_string(vrs));
		string _query("SELECT File_CL, Last_Modif, Hash FROM '" + table_name + "' WHERE Versione_BCK = ?;");
		SQLite::Statement query(db, _query);
		query.bind(1, vrs);
		// Dato il numero di backup desiderato inviare per ogni file:
		int count = db.execAndGet("SELECT COUNT (*) FROM '" + table_name + "' WHERE Versione_BCK =" + to_string(vrs) + ";");
		for (int i = 0; i < count; i++) {
			query.executeStep();
			string nome = query.getColumn("File_CL");
			time_t timestamp = query.getColumn("Last_Modif");
			string hash = query.getColumn("Hash");
			// [Percorso completo]\r\n[Ultima modifica -> 8byte][Hash -> 32char]\r\n
			int cur_file_len = nome.length() + 2/*\r\n*/+ 8 * /*Ultima modifica*/+32/*Hash*/+ 2/*\r\n*/+ 1/*\0*/;
			char cur_file[cur_file_len];
			sprintf(cur_file, "%s\r\n%016lx%s\r\n", nome.c_str(), timestamp, hash.c_str());
			/*string to_send(nome + "\r\n" + to_string(timestamp) + hash + "\r\n");*/
			send(s_c, cur_file, cur_file_len, 0);
			len = recv(s_c, buf, COMM_LEN, 0);
			if ((len == 0) || (len == -1)) {
				// Ricevuta stringa vuota: connessione persa
				throw runtime_error("connessione persa");
			}
			buf[len] = '\0';
			if (!strcmp(buf, "DATA_REC")) {
				Logger::write_to_log("Informazioni file inviate", DEBUG, CONSOLE_ONLY);
				///* DEBUG */cout << "- Informazioni file inviate" << endl;
			}

		}
		Logger::write_to_log("Informazioni relative alla versione " + to_string(vrs) + " inviate correttamente");
		return;
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		///* DEBUG */cerr << "- Errore DB: " << e.what() << endl;
		send_command(s_c, "DB_ERROR");
		return;
	}
}

void Folder::remove_files() {
	files.clear();
}

bool Folder::new_file_backup(int s_c) {

	if (!isSelected()) {
		throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
	}

	int vrs;
	// Un nuovo file deve essere inserito nel backup corrente:
	// - Si trova la versione di backup corrente
	try {
		SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
		string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
		SQLite::Statement query(db, _query);
		if (query.executeStep()) {
			// Un risultato = corretto, si aggiunge all'ultimo backup
			vrs = query.getColumn(0).getInt() + 1;
		} else {
			// Non esiste un backup completo corrente -> Errore
			Logger::write_to_log("Si sta tentando di aggiungere un file senza avere un backup completo", ERROR);
			///* DEBUG */cerr << "- Si sta tentando di aggiungere un file senza avere un backup completo" << endl;
			send_command(s_c, "MISS_BCK");
			return false;
		}
		query.reset();
		// - Si apre un backup e una transazione sul DB
		Backup bck(vrs, (*this));
		SQLite::Transaction trs(db);
		// - Si copiano le voci relative all'ultima versione in una nuova versione con gli stessi file
		SQL_copy_rows(db, (vrs - 1));
		// - Si riceve il file [receive_file]
		for (int tent = 1; tent < 6;) {
			if (receive_file(s_c, vrs, db, string())) {
				// - Se il file e le informazioni sul DB sono state processate correttamente
				// - Commit DB e backup completato
				send_command(s_c, "DATA_OK_");
				trs.commit();
				bck.completed();
				Logger::write_to_log("Ricezione file completata");
				///* DEBUG */cout << "- Ricezione file completata" << endl;
				return true;
			} else {
				///* DEBUG */cerr << "- Errore nella ricezione: tentativo " << to_string(tent) << endl;
				if (errno != EEXIST) {
					Logger::write_to_log("Errore nella ricezione: tentativo " + to_string(tent), ERROR);
					send_command(s_c, "SNDAGAIN");
					tent++;
					continue;
				} else {
					Logger::write_to_log("File già esistente", ERROR);
					///* DEBUG */cerr << "- File già esistente" << endl;
					send_command(s_c, "DATA_OK_");
					trs.commit();
					bck.completed();
					return true;
				}
			}
		}
		return false;
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		///* DEBUG */cerr << "- Errore DB: " << e.what() << endl;
		send_command(s_c, "DB_ERROR");
		return false;
	}
}

bool Folder::receive_file(int s_c, int vrs, SQLite::Database& db, string folder_name) {

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
		// Ricevuta stringa vuota: connessione persa
		throw runtime_error("connessione persa");
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

	File file(filename, (*this), size, timestamp, hash);
	///* DEBUG */cout << "Nome file: " << file.getName() << endl;
	///* DEBUG */cout << "nella cartella " << file.getBasePath().getPath() << endl;
	///* DEBUG */cout << "di dimensione " << file.getSize() << " Byte" << endl;
	///* DEBUG */cout << "Percorso completo file: " << file.getFullPath() << endl;
	///* DEBUG */timestamp = file.getTimestamp();
	///* DEBUG */struct tm *time_info = gmtime(&timestamp);
	///* DEBUG */time_info->tm_hour+=2;
	///* DEBUG */strftime(buffer, MAX_BUF_LEN, "%A %e %B %Y, %H:%M:%S", time_info);
	///* DEBUG */cout << "ultima modifica: " << buffer << endl;
	try {
		// - Elimina informazioni relative al file dalla versione corrente del backup, se presenti
		// SQL: DELETE FROM '[nome_table]' WHERE Versione_BCK=[vrs] AND Hash=file.getHash()
		string _query("DELETE FROM '" + table_name + "' WHERE Versione_BCK=" + to_string(vrs) + " AND File_CL='" + file.getName() + "';");
		SQLite::Statement query(db, _query);
		Logger::write_to_log("File: " + file.getFullPath() + ", Dimensione: " + to_string(file.getSize()) + " Byte, MD5: " + string(file.getHash()), DEBUG, LOG_ONLY);
		if (query.exec() == 0) {
			// Non è un aggiornamento, ma un inserimento
			Logger::write_to_log("Il file non è un aggiornamento, ma un file nuovo", DEBUG, LOG_ONLY);
			///* DEBUG */cout << "- Il file non è un aggiornamento, ma un file nuovo" << endl;
		} else {
			Logger::write_to_log("Il file è un aggiornamento", DEBUG, LOG_ONLY);
			///* DEBUG */cout << "- Il file è un aggiornamento" << endl;
		}
		// Invio conferma ricezione dati
		send_command(s_c, "INFO_OK_");
		// Ricezione contenuto del file
		if (file.receive_file_data(s_c, folder_name)) {
			/* Inserimento file nel database */;
			string _query("INSERT INTO [" + table_name + "] (File_CL, Last_Modif, File_SRV, Hash, Versione_BCK) VALUES (?, ?, ?, ?, ?);");
			SQLite::Statement query(db, _query);
			query.bind(1, file.getName());
			query.bind(2, (long long int) file.getTimestamp());
			query.bind(3, file.save_path);
			query.bind(4, file.getHash());
			query.bind(5, vrs);
			if (query.exec() == 1) {
				Logger::write_to_log("Entry relativa al file inserita con successo");
				///* DEBUG */cout << "- Entry relativa al file inserita con successo" << endl;
				file.completed();
				insert_file(file);
				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
	} catch (ios_base::failure &e) {
		Logger::write_to_log("Errore nella scrittura del file: " + string(e.what()), ERROR);
		///* DEBUG */cerr << "- Errore nella scrittura del file: " << e.what() << endl;
		return false;
	}
}

void Folder::SQL_copy_rows(SQLite::Database& db, int vrs) {
//	CREATE TABLE 'temporary_table' AS SELECT * FROM '[nome_table]' WHERE Versione_BCK= ?;
	string table("temporary_table" + user.getUser());
	string _query = "CREATE TABLE '" + table + "' AS SELECT * FROM '" + table_name + "' WHERE Versione_BCK= ?;";
	SQLite::Statement query_1(db, _query);
	query_1.bind(1, vrs);
	query_1.exec();
//	UPDATE 'temporary_table' SET Versione_BCK=(Versione_BCK+1);
	_query = "UPDATE '" + table + "' SET Versione_BCK=(Versione_BCK+1);";
	SQLite::Statement query_2(db, _query);
	query_2.exec();
//	UPDATE 'temporary_table' SET File_ID=NULL;
	_query = "UPDATE '" + table + "' SET File_ID=NULL;";
	SQLite::Statement query_3(db, _query);
	query_3.exec();
//	INSERT INTO '[nome_table]' SELECT * FROM 'temporary_table';
	_query = "INSERT INTO '" + table_name + "' SELECT * FROM '" + table + "';";
	SQLite::Statement query_4(db, _query);
	query_4.exec();
//	DROP TABLE 'temporary_table';
	_query = "DROP TABLE '" + table + "';";
	SQLite::Statement query_5(db, _query);
	query_5.exec();
}

bool Folder::delete_file_backup(int s_c) {
	// Un file deve essere eliminato dal backup corrente:
	// - Si trova la versione di backup corrente
	if (!isSelected()) {
		throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
	}
	int vrs;
	char buffer[SHORT_BUF_LEN];
	try {
		SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
		string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
		SQLite::Statement query(db, _query);
		if (query.executeStep()) {
			// Un risultato = corretto, si aggiunge all'ultimo backup
			vrs = query.getColumn(0).getInt() + 1;
		} else {
			// Non esiste un backup completo corrente -> Errore
			Logger::write_to_log("Non esiste un backup completo corrente", ERROR);
			///* DEBUG */cerr << "- Si sta tentando di eliminare un file senza avere un backup completo" << endl;
			send_command(s_c, "MISS_BCK");
			return false;
		}
		query.reset();
		Logger::write_to_log("Backup trovato, versione corrente: " + to_string(vrs - 1));
		// - Si apre una transazione sul DB
		SQLite::Transaction trs(db);
		// - Si copiano le voci relative all'ultima versione in una nuova versione con gli stessi file
		SQL_copy_rows(db, (vrs - 1));
		// - Ricezione nome del file da eliminare
		int len;
		len = recv(s_c, buffer, SHORT_BUF_LEN, 0);
		if ((len == 0) || (len == -1)) {
			// Ricevuta stringa vuota: connessione persa
			throw runtime_error("connessione persa");
		}
		buffer[len] = '\0';
		Logger::write_to_log("File da eliminare: " + string(buffer));
		send_command(s_c, "INFO_OK_");
		_query.assign("DELETE FROM '" + table_name + "' WHERE File_CL = ? AND Versione_BCK = ?;");
		SQLite::Statement query_1(db, _query);
		query_1.bind(1, buffer);
		query_1.bind(2, vrs);
		// - Nella versione nuova si elimina il file richiesto
		if (query_1.exec() == 1) {
			// Eliminazione corretta di UNA SOLA riga
			trs.commit();
			Logger::write_to_log("File rimosso correttamente dal backup corrente");
			return true;
		} else {
			// Eliminazione di più di una/nessuna riga della tabella
			Logger::write_to_log("File non esistente", ERROR);
			return false;
		}
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		send_command(s_c, "DB_ERROR");
		return false;
	}
	return false;
}

bool Folder::full_backup(int s_c) {

	if (!isSelected()){
		throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
	}

	int vrs, len;
	char comm[COMM_LEN];

	// Creazione nuova versione backup nel DB
	// 		// Lettura versione attuale
	try {
		SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
		string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
		SQLite::Statement query(db, _query);
		// Esecuzione query -> una sola riga di risultato (SELECT MAX)

		if (query.executeStep())
			// Se c'è un risultato = esiste già un backup della cartella
			vrs = query.getColumn(0).getInt() + 1;
		else
			// Altrimenti: prima versione del backup
			vrs = 1;
		// Ricevere tutti  i file che il client mi manda, inserendoli in ordine di arrivo nel DB, con la versione corretta
		query.reset();

		Backup backup(vrs, (*this));
		SQLite::Transaction trs(db);
		string folder_name("./ReceivedFiles/" + user.getUser() + "_" + path + "_FullBackup_" + to_string(time(NULL)));

		// Attesa comando: 	- se REC_FILE -> nuovo file in arrivo
		//					- se SYNC_END -> fine backup
		while (1) {
			len = recv(s_c, comm, COMM_LEN, 0);
			if ((len == 0) || (len == -1)) {
				// Ricevuta stringa vuota: connessione persa
				throw runtime_error("connessione persa");
			}
			comm[len] = '\0';
			if (!strcmp(comm, "REC_FILE")) {
				// Ricevi file
				if (receive_file(s_c, vrs, db, folder_name)) {
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
				send_command(s_c, "UNKNOWN_");
				break;
			}
		}
		if (backup.isComplete() && trs.isCommitted()) {
			// Se la transazione ha avuto commit e il backup è completo
			Logger::write_to_log("Backup ricevuto con successo");
			///* DEBUG */cout << "Backup ricevuto con successo" << endl;
			db_maintenance(table_name);
			return true;
		} else {
			// Se la transazione non ha avuto commit
			// Rollback e cancellazione file
			Logger::write_to_log("Errore nella ricezione del backup, annullamento operazione");
			///* DEBUG */cerr << "Errore nella ricezione del backup, annullamento operazione" << endl;
			return false;
		}
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		///* DEBUG */cerr << "- Errore DB: " << e.what() << endl;
		send_command(s_c, "DB_ERROR");
		return false;
	}
}

void Folder::send_backup(int s_c){
	//TODO Implementare funzione di invio backup completo
	// Invio backup completo:
	// - Si inviano all'utente le informazioni su TUTTE le versioni presenti sul server, separate per numero di versione
	// - Si attende che l'utente selezioni la versione desiderata
	// - Si spedisce la versione selezionata, un file per volta
	return;
}

void Folder::send_single_file(int s_c){
	//TODO Scegliere modalità e implementare funzione di invio file singolo da backup
	// Invio file singolo da backup:
	// 2 possibilità:
	// 1°:
	// - Si attende di il nome del file richiesto dall'utente
	// - Si trovano nel DB tutte le versioni in cui compare il file, mostrando solo come diverse quelle con diverso MD5
	// - Si attende la versione richiesta dall'utente
	// - Si spedisce il file richiesto
	// 2°:
	// - Si elencano tutti i file in ogni versione del backup
	// - Si attende di il nome del file richiesto dall'utente, con la versione relativa
	// - Si spedisce il file richiesto
}
