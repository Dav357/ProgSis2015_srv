//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================
#include "GenIncludes.hpp"

#include <map>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "md5.hpp"

#define BKLOG 10
#define FILE_NAME_LEN 30

using namespace std;

enum StringValue {
	Undefined, Select_folder, Clear_folder, Receive_file, Logout
};

static map<string, StringValue> m_CommandsValues;

void terminate_handler() {
	cerr << "Chiusura forzata processo\n";
	exit(-1);
}

void server_function(int, int);
static void Initialize() {
	m_CommandsValues["SET_FOLD"] = Select_folder;
	m_CommandsValues["CLR_FOLD"] = Clear_folder;
	m_CommandsValues["REC_FILE"] = Receive_file;
	m_CommandsValues["LOGOUT__"] = Logout;
	///* DEBUG*/cout << "m_CommandsValues contiene " << m_CommandsValues.size() << " elementi." << endl;
}



int main(int argc, char** argv) {

	int port, pid;
	struct sockaddr_in saddr, claddr;
	socklen_t claddr_len = sizeof(struct sockaddr_in);
	int s, s_c; //Socket
	MD5 md5;

	Initialize();

	//SQLite::Database db("provabd.db3", SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE);

	if (argc != 2) {
		cerr << "- Errore nei parametri\n- Formato corretto: <porta>\n" << endl;
		return -1;
	}

	port = atoi(argv[1]);

	// Creazione socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		cerr << "- Errore nella crezione del socket, chiusura programma" << endl;
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
		cerr << "- Errore nell'operazione di ascolto sul socket, chiusura programma" << endl;
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

	Account ac;
	Folder f;
	int len;
	char buffer[MAX_BUF_LEN + 1] = "", comm[COMM_LEN + 1] = "";

	// Prima oprerazione: login;
	strcpy(buffer, "Inserire dati utente o richiesta nuovo account");
	send(s_c, buffer, strlen(buffer), 0);
	if (!((ac = login(s_c)).is_complete())) {
		cerr << "- [" << pid << "] Errore nella procedura di autenticazione" << endl;
		cerr << "- [" << pid << "] Chiusura connessione" << endl;
		close(s_c);
		return;
		///* DEBUG */exit(-1);
	}

	while (ac.is_complete()) {
		// Attesa comando dal client
		cout << "- [" << pid << "] In attesa di comandi dal client" << endl;
		len = recv(s_c, comm, COMM_LEN, 0);

		///* DEBUG */cout << comm << endl;
		///* DEBUG */cout << m_CommandsValues[comm] << endl;
		switch (m_CommandsValues[comm]) {
		case Select_folder:
			comm[0] = '\0';
			cout << "- [" << pid << "] Ricevuto comando 'seleziona cartella'" << endl;
			cout << "- [" << pid << "] In attesa del percorso" << endl;
			f = select_folder(s_c, ac.getUser());
			cout << "- [" << pid << "] Cartella selezionata: " << f.getPath() << endl;
			break;
		case Clear_folder:
			comm[0] = '\0';
			cout << "- [" << pid << "] Ricevuto comando 'deseleziona cartella'" << endl;
			f.clear_folder();
			cout << "- [" << pid << "] Cartella deselezionata" << endl;
			break;
		case Receive_file:
			comm[0] = '\0';
			cout << "- [" << pid << "] Ricevuto comando 'ricevi file'" << endl;
			cout << "- [" << pid << "] In attesa del file" << endl;
			receive_file(s_c, f);
			break;
		case Logout:
			comm[0] = '\0';
			cout << "- [" << pid << "] Ricevuto comando 'logout'" << endl;
			ac.clear();
			return;
			break;
		default:
			break;
		}
	}

	return;
}

void send_command(int s_c, const char *command) {
	char buffer[COMM_LEN + 1];
	strcpy(buffer, command);
	send(s_c, buffer, strlen(buffer), 0);
}

