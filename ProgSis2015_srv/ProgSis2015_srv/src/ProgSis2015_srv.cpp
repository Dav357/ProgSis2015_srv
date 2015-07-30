//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "SQLiteCpp/SQLiteCpp.h"
#include "md5.h"

#define BKLOG 10
#define COMM_LEN 35
#define FILE_NAME_LEN 30

using namespace std;

void terminate_handler() {
	cerr << "Chiusura forzata processo\n";
	exit(-1);
}

void server_function(int);

int main(int argc, char** argv) {
	int port, result;
	char comm[COMM_LEN], nmfile[FILE_NAME_LEN];
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
		cout << "- Connesso all'host: " << inet_ntoa(claddr.sin_addr) << endl << "- In attesa di comandi\n";
		server_function(s_c);
	}

	close (s);
	return 0;
}

// Funzione di gestione operazioni del server
void server_function(int s_c){



	close(s_c);
	return;
}

