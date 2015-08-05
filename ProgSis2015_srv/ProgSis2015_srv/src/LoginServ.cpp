#include "GenIncludes.hpp"

using namespace std;

/* Funzioni classe Account */
Account::Account() {
	usr = "";
	shapass = "";
	complete = false;
}
Account::Account(string username, string password) {
	usr = username;
	shapass = password;
	complete = true;
}

void Account::assign_username(std::string username) {
	usr = username;
	if (!shapass.empty()) {
		complete = true;
	}
}
void Account::assign_username(char* username) {
	usr.assign(username);
	if (!shapass.empty()) {
		complete = true;
	}

}
void Account::assign_password(std::string password) {
	shapass = password;
	if (!usr.empty()) {
		complete = true;
	}
}
void Account::assign_password(char* password) {
	shapass.assign(password);
	if (!usr.empty()) {
		complete = true;
	}
}
bool Account::bind_to_query(SQLite::Statement& q) {
	if (complete) {
		q.bind(1, usr);
		q.bind(2, shapass);
		return true;
	}
	return false;
}
void Account::clear() {
	usr.clear();
	shapass.clear();
	complete = false;
}
/* Fine funzioni classe Account*/

Account login(int s_c) {

	int len, flags;
	int pid = getpid();
	char *buf;
	string usr, shapass;
	Account ac;
	char buffer[MAX_BUF_LEN] = "", comm[COMM_LEN + 1] = "";

	len = recv(s_c, comm, COMM_LEN, 0);
	buffer[len] = '\0';

	try {
		// Richiesta di login o richiesta di creazione account
		if (!strcmp(comm, "LOGIN___") || (!strcmp(comm, "CREATEAC"))) {
			cout << "- [" << pid << "] Autenticazione in corso" << endl;
			cout << "- [" << pid << "] In attesa dei dati di accesso" << endl;
			// Ricezione "[username]\r\n[sha-256_password]\r\n"
			len = read(s_c, buffer, MAX_BUF_LEN);
			buffer[len] = '\0';
			// Inserimento in stringhe di [username]...
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
			if (account_manag(ac, flags)) {
				strcpy(buffer, "Accesso effettuato");
			} else {
				strcpy(buffer, "Accesso non effettauto");
			}
			send(s_c, buffer, strlen(buffer), 0);
			return ac;
		} else {
			// Comando non riconosciuto
			strcpy(buffer, "Comando non riconosciuto");
			send(s_c, buffer, strlen(buffer), 0);
			return ac;
		}
	} catch (SQLite::Exception& e) {
		strcpy(buffer, "Errore nell'accesso al database");
		send(s_c, buffer, strlen(buffer), 0);
		return ac;
	}
}

bool account_manag(Account& ac, int flags) {

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
		ac.bind_to_query(query);
		if (flags == LOGIN) {
			if (query.executeStep()) {
				cout << "Account trovato" << endl;
				return true;
			} else {
				cerr << "Account non trovato" << endl;
				ac.clear();
				return false;
			}
		} else {
			if (query.exec() == 1) {
				cout << "Account creato con successo" << endl;
				return true;
			} else {
				cerr << "Errore nella creazione dell'account" << endl;
				ac.clear();
				return false;
			}
		}
	} catch (Exception& e) {
		ac.clear();
		throw e;
	}

}

void logout(int s_c){


}


