#include "GenIncludes.hpp"

using namespace std;

bool login(int s_c) {

	bool res = false;
	int len, flags;
	int pid = getpid();
	char *buf;
	string usr, shapass;
	char buffer[MAX_BUF_LEN] = "", comm[COMM_LEN + 1] = "";

	len = recv(s_c, comm, COMM_LEN, 0);
	buffer[len] = '\0';

	try {
		// Richiesta di login o richiesta di creazione account
		if (!strcmp(comm, "LOGIN___") || (!strcmp(comm, "CREATEAC"))) {
			cout << "- [" << pid << "]Autenticazione in corso" << endl;
			cout << "- [" << pid << "]In attesa dei dati di accesso" << endl;
			// Ricezione "[username]\r\n[sha-256_password]\r\n"
			len = read(s_c, buffer, MAX_BUF_LEN);
			buffer[len] = '\0';
			// Inserimento in stringhe di [username]...
			buf = strtok(buffer, "\r\n");
			usr.assign(buf);
			// ... e dell'SHA-256 della password
			buf = strtok(NULL, "\r\n");
			shapass.assign(buf);
			if (!strcmp(comm, "LOGIN___")) {
				flags = LOGIN;
			} else {
				flags = CREATEACC;
			}
			if (account_manag(usr, shapass, flags)) {
				strcpy(buffer,"Accesso effettuato");
				res = true;
			} else {
				strcpy(buffer, "Accesso non effettauto");
				res = false;
			}
			send(s_c, buffer, strlen(buffer), 0);
			return res;

		} else if (!strcmp(buffer, "LOGOUT__")) {
			// Richiesta di logout
			strcpy(buffer, "Logout");
			send(s_c, buffer, strlen(buffer), 0);
			return false;

		} else {
			// Comando non riconosciuto
			strcpy(buffer, "Comando non riconosciuto");
			send(s_c, buffer, strlen(buffer), 0);
			return false;
		}
	} catch (SQLite::Exception& e) {
		strcpy(buffer, "Errore nell'accesso al database");
		send(s_c, buffer, strlen(buffer), 0);
		return false;

	}
}

int account_manag(std::string usr, std::string shapass, int flags) {

	// flags vale 0x00000001 per login (equivale a DB in sola lettura) o 0x00000002 per creazione account (DB in lettura/scrittura)
	using namespace SQLite;
	string _query;
	try {
		Database db("database.db3", flags);
		///* DEBUG */cout << "SQLite database file '" << db.getFilename() << "' opened successfully with flags: " << flags << endl;
		///* DEBUG */bool bExists = db.tableExists("users");
		///* DEBUG */cout << "SQLite table 'test' exists=" << bExists << endl;
		if (flags == LOGIN) {
			/* Login */
			_query.assign(
					"SELECT * FROM users WHERE username = ? AND sha2_pass = ?");
		} else {
			/* Creazione account */
			_query.assign(
					"INSERT INTO users (username, sha2_pass) VALUES (?, ?)");
		}

		Statement query(db, _query);
		///* DEBUG */ cout << "SQLite statement '" << query.getQuery() << "' compiled (" << query.getColumnCount() << " columns in the result)" << endl;
		query.bind(1, usr);
		query.bind(2, shapass);
		if (flags == LOGIN) {
			if (query.executeStep()) {
				cout << "Account trovato" << endl;
				return true;
			} else {
				cerr << "Account non trovato" << endl;
				return -1;
			}
		} else {
			if (query.exec() == 1) {
				cout << "Account creato con successo" << endl;
				return true;
			} else {
				cerr << "Errore nella creazione dell'account" << endl;
				return -1;
			}
		}
	} catch (Exception& e) {
		throw e;
		return -2;
	}

}

