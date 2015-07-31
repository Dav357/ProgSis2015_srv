//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================

#include <iostream>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "SQLiteCpp/SQLiteCpp.h"

#include "md5.hpp"

#define LOGIN 0x0001
#define CREATEACC 0x0002
#define BKLOG 10
#define MAX_BUF_LEN 1024
#define COMM_LEN 8
#define FILE_NAME_LEN 30

using namespace std;

void terminate_handler() {
	cerr << "Chiusura forzata processo\n";
	exit(-1);
}

void clear_cstr(char *toclear, int len) {
	memset(toclear, '\0', len);
}
void server_function(int);
bool login(int);
bool account_manag(string usr, string shapass, int flag);

int main(int argc, char** argv) {
	int port;
	struct sockaddr_in saddr, claddr;
	socklen_t claddr_len = sizeof(struct sockaddr_in);
	int s, s_c; //Socket
	MD5 md5;

	//SQLite::Database db("provabd.db3", SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE);

	if (argc != 2) {
		cerr << "- Errore nei parametri\n- Formato corretto: <porta>\n" << endl;
		return -1;
	}

	port = atoi(argv[1]);

	// Creazione socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		cerr << "- Errore nella crezione del socket, chiusura programma"
				<< endl;
		return -1;
	};
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);

	// Bind del socket e listen
	if (bind(s, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
		cerr << "- Errore nel binding del socket, chiusura programma" << endl;
		return -1;
	}
	if (listen(s, BKLOG) == -1) {
		cerr
				<< "- Errore nell'operazione di ascolto sul socket, chiusura programma"
				<< endl;
		return -1;
	}

	while (1) {
		cout << "- In attesa di connessioni" << endl;
		s_c = accept(s, (struct sockaddr *) &claddr, &claddr_len);
		cout << "- Connesso all'host: " << inet_ntoa(claddr.sin_addr) << endl
				<< "- In attesa di comandi\n";
		server_function(s_c);
	}

	close(s);
	return 0;
}

// Funzione di gestione operazioni del server
void server_function(int s_c) {

	char buffer[MAX_BUF_LEN] = "";

	cout << getpid() << endl;
	// Prima oprerazione: login;
	strcpy(buffer, "Inserire dati utente o richiesta nuovo account");
	send(s_c, buffer, strlen(buffer), 0);
	if (!login(s_c)) {
		cerr << "Errore nella procedura di autenticazione" << endl;
		exit(-1);
	}

	getchar();
	close(s_c);
	return;
}

bool login(int s_c) {

	int len;
	int pid = getpid();
	char *buf;
	string usr, shapass;
	char buffer[MAX_BUF_LEN] = "", comm[COMM_LEN + 1] = "";

	len = read(s_c, comm, COMM_LEN);
	buffer[len] = '\0';
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
			return account_manag(usr, shapass, LOGIN);
		} else {
			return account_manag(usr, shapass, CREATEACC);
		}
	} else if (!strcmp(buffer, "LOGOUT__")) {
		// Richiesta di logout
		return false;

	} else {
		// Comando non riconosciuto
		return false;
	}
}

bool account_manag(string usr, string shapass, int flag) {

	using namespace SQLite;
	Database db("database.db3");
	cout << "SQLite database file '" << db.getFilename() << "' opened successfully" << endl;
	bool bExists = db.tableExists("users");
	cout << "SQLite table 'test' exists=" << bExists << endl;
	Statement query(db,	"SELECT * FROM users WHERE username = ? and sha2_pass = ?");
	cout << "SQLite statement '" << query.getQuery() << "' compiled (" << query.getColumnCount() << " columns in the result)" << endl;
	query.bind(1, usr);
	query.bind(2, shapass);
	if (!query.executeStep()){
		cerr << "Account non trovato"<< endl;
		return false;
	} else {
		cout << "Account trovato" << endl;
		return true;
	}
}
