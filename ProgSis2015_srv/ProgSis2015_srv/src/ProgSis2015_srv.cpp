//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================
#include "GenIncludes.hpp"

#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "md5.hpp"

#define BKLOG 10
#define FILE_NAME_LEN 30

using namespace std;

void terminate_handler() {
	cerr << "Chiusura forzata processo\n";
	exit(-1);
}

void clear_cstr(char *toclear, int len) {
	memset(toclear, '\0', len);
}

void server_function(int, int);

int main(int argc, char** argv) {

	int port, pid;
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
		/* Padre: in attesa di connessioni */
		cout << "- In attesa di connessioni" << endl;

		/* Ogni figlio riceve una connessione -> differ. per PID*/
		s_c = accept(s, (struct sockaddr *) &claddr, &claddr_len);
		pid = getpid();
		cout << "- [" << pid << "] Connesso all'host: " << inet_ntoa(claddr.sin_addr) << endl;
		cout << "- [" << pid << "] In attesa di comandi" << endl;
		server_function(s_c, pid);
	}

	close(s);
	return 0;
}

// Funzione di gestione operazioni del server
void server_function(int s_c, int pid) {

	char buffer[MAX_BUF_LEN] = "";

	// Prima oprerazione: login;
	strcpy(buffer, "Inserire dati utente o richiesta nuovo account");
	send(s_c, buffer, strlen(buffer), 0);
	if (!login(s_c)) {
		cerr << "- [" << pid << "] Errore nella procedura di autenticazione" << endl;
		close(s_c);
		return;
		///* DEBUG */exit(-1);
	}

	getchar();
	return;
}
