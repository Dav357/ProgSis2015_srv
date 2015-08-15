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

	if ((len = recv(s_c, comm, COMM_LEN, 0)) == 8) {
		try {
			// Richiesta di login o richiesta di creazione account
			if (!strcmp(comm, "LOGIN___") || (!strcmp(comm, "CREATEAC"))) {
				cout << "- [" << pid << "] Autenticazione in corso" << endl;
				cout << "- [" << pid << "] In attesa dei dati di accesso" << endl;
				// Ricezione "[username]\r\n[sha-256_password]\r\n"
				len = recv(s_c, buffer, MAX_BUF_LEN, 0);
				if (len != 0 && len != -1) {
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
					if (account_manag(ac, flags)) {
						send_command(s_c, "LOGGEDOK");
					} else {
						send_command(s_c, "LOGINERR");
					}
					return ac;
				} else {
					send_command(s_c, "LOGINERR");
					ac.clear();
					return ac;
				}
			} else {
				// Comando non riconosciuto
				send_command(s_c, "UNKNOWN_");
				return ac;
			}
		} catch (SQLite::Exception& e) {
			send_command(s_c, "DB_ERROR");
			return ac;
		}
	} else {
		/* DEBUG */cerr << "Errore nella ricezione del comando" << endl;
		return ac;
	}
}

bool account_manag(Account& ac, int flags) {

	// flags vale 0x00000001 per login (equivale a DB in sola lettura) o 0x00000002 per creazione account (DB in lettura/scrittura)
	using namespace SQLite;
	string _query;
	try {
		Database db("database.db3", flags);
		if (flags == LOGIN) {
			/* Login */
			_query.assign("SELECT * FROM users WHERE username = ? AND sha2_pass = ?");
		} else {
			/* Creazione account */
			_query.assign("INSERT INTO users (username, sha2_pass) VALUES (?, ?)");
		}
		Statement query(db, _query);
		ac.bind_to_query(query);
		if (flags == LOGIN) {
			if (query.executeStep()) {
				/* DEBUG */cout << "Account trovato" << endl;
				return true;
			} else {
				/* DEBUG */cerr << "Account non trovato" << endl;
				ac.clear();
				return false;
			}
		} else {
			if (query.exec() == 1) {
				/* DEBUG */cout << "Account creato con successo" << endl;
				return true;
			} else {
				/* DEBUG */cerr << "Errore nella creazione dell'account" << endl;
				ac.clear();
				return false;
			}
		}
	} catch (Exception& e) {
		ac.clear();
		throw e;
	}

}

void logout(int s_c) {

}

