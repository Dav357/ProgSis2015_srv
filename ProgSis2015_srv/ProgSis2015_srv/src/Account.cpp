#include "Account.h"

#include "GenIncludes.hpp"

using namespace std;

/* Costruttori */

// Costruttore parziale, per inizializzazione
Account::Account() {
	usr = "";
	shapass = "";
	complete = false;
}

// Costruttore completo
Account::Account(string username, string password) {
	usr = username;
	shapass = password;
	complete = true;
}
/* Fine costruttori */

// Assegnazione nome utente all'account corrente (da stringa)
void Account::assign_username(std::string username) {
	usr = username;
	if (!shapass.empty()) {
		complete = true;
	}
}

// Assegnazione nome utente all'account corrente (da puntatore a char)
void Account::assign_username(char* username) {
	usr.assign(username);
	if (!shapass.empty()) {
		complete = true;
	}
}

// Assegnazione password all'account corrente (da stringa)
void Account::assign_password(std::string password) {
	shapass = password;
	if (!usr.empty()) {
		complete = true;
	}
}

// Assegnazione password all'account corrente (da puntatore a char)
void Account::assign_password(char* password) {
	shapass.assign(password);
	if (!usr.empty()) {
		complete = true;
	}
}

// Rimozione dei dati relativi all'account
void Account::clear() {
	if (!usr.empty())
		remove_from_usertable(usr);
	usr.clear();
	shapass.clear();
	complete = false;

}

// Gestione account: login/creazione
bool Account::account_manag(int flags) {
	// flags vale:
	// - 0x01 per login (equivale a DB in sola lettura)
	// - 0x02 per creazione account (DB in lettura/scrittura)
	string _query;
	SQLite::Database db("database.db3", flags);
	if (flags == LOGIN) {
		/* Login */
		_query.assign("SELECT * FROM users WHERE username = ? AND sha2_pass = ?");
	} else {
		/* Creazione account */
		_query.assign("INSERT INTO users (username, sha2_pass) VALUES (?, ?)");
	}
	SQLite::Statement query(db, _query);
	query.bind(1, usr);
	query.bind(2, shapass);
	if (flags == LOGIN) {
		if (query.executeStep()) {
			Logger::write_to_log("Account " + usr + " trovato");
			return (add_to_usertable(usr));
		} else {
			Logger::write_to_log("Account " + usr + " non trovato");
			clear();
			return false;
		}
	} else {
		if (query.exec() == 1) {
			Logger::write_to_log("Account " + usr + " creato con successo, utente connesso");
			return (add_to_usertable(usr));
		} else {
			Logger::write_to_log("Errore nella creazione dell'account", ERROR);
			clear();
			return false;
		}
	}
}

// Selezione di una cartella relativa all'account
Folder Account::select_folder(int s_c) {

	int len;
	char buffer[MAX_BUF_LEN+1] = "";
	string path;

	len = recv(s_c, buffer, MAX_BUF_LEN, 0);
	if ((len == 0) || (len == -1)) {
		// Ricevuta stringa vuota: connessione persa
		throw runtime_error("connessione persa");
	}
	buffer[len] = '\0';
	path.assign(buffer);
	// Costruzione nome tabella relativa alla cartella considerata
	Folder f(path, (*this));
	try {
		// Apertura DB (sola lettura)
		SQLite::Database db("database.db3", SQLITE_OPEN_READONLY);

		if (db.tableExists(f.getTableName())) {
			Logger::write_to_log("Trovata tabella relativa alla cartella");
			send_command(s_c, "FOLDEROK");
			return f;
		} else {
			Logger::write_to_log("Tabella relativa alla cartella " + path + " non trovata, creazione tabella");
			if (f.create_table_folder()) {
				send_command(s_c, "FOLDEROK");
				return f;
			} else {
				send_command(s_c, "FOLD_ERR");
				return (Folder());
			}
		}
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		send_command(s_c, "DB_ERROR");
		return (Folder());
	}
	return (Folder());
}

// Funzione di login
Account login(int s_c, char *usertable) {

	int len, flags;
	char *buf;
	string usr, shapass;
	Account ac;
	char buffer[MAX_BUF_LEN+1] = "", comm[COMM_LEN + 1] = "";
	len = recv(s_c, comm, COMM_LEN, 0);
	if ((len == 0) || (len == -1)) {
		// Ricevuta stringa vuota: connessione persa
		throw runtime_error("connessione persa");
	}
	comm[len] = '\0';
	try {
		// Richiesta di login o richiesta di creazione account
		if (!strcmp(comm, "LOGIN___") || (!strcmp(comm, "CREATEAC"))) {
			Logger::write_to_log("Autenticazione in corso, in attesa dei dati di accesso");
			// Ricezione "[username]\r\n[sha-256_password]\r\n"
			len = recv(s_c, buffer, MAX_BUF_LEN, 0);
			if ((len == 0) || (len == -1)) {
				// Ricevuta stringa vuota: connessione persa
				throw runtime_error("connessione persa");
			}
			buffer[len] = '\0';
			// Inserimento in stringhe di username...
			buf = strtok(buffer, "\r\n");
			ac.assign_username(buf);
			// ... e dell'SHA-256 della password
			buf = strtok(NULL, "\r\n");
			ac.assign_password(buf);
			if (!strcmp(comm, "LOGIN___")) {
				flags = LOGIN;
			} else {
				flags = CREATEACC;
			}
			if (ac.account_manag(flags)) {
				send_command(s_c, "LOGGEDOK");
				Logger::write_to_log("Utente " + ac.getUser() + " connesso");
				return ac;
			} else {
				send_command(s_c, "LOGINERR");
				return (Account());
			}
		} else {
			// Comando non riconosciuto
			send_command(s_c, "UNKNOWN_");
			return ac;
		}
	} catch (SQLite::Exception& e) {
		Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
		send_command(s_c, "DB_ERROR");
		ac.clear();
		return ac;
	}
	return (Account());
}
