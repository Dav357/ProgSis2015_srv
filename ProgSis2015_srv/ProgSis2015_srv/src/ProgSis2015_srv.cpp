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
	Undefined, Select_folder, Clear_folder, Receive_file, Receive_full_backup, Send_full_backup, New_file_current_backup, Del_file_current_backup, Alter_file_current_backup, Get_current_files, Logout
};

static map<string, StringValue> m_CommandsValues;

void server_function(int, int);

static void Initialize() {
	m_CommandsValues["SET_FOLD"] = Select_folder;
	m_CommandsValues["CLR_FOLD"] = Clear_folder;
	m_CommandsValues["REC_FILE"] = Receive_file;
	m_CommandsValues["SYNC_RCV"] = Receive_full_backup;
	m_CommandsValues["FLD_STAT"] = Get_current_files;

	m_CommandsValues["NEW_FILE"] = New_file_current_backup;
	m_CommandsValues["DEL_FILE"] = Del_file_current_backup;
	m_CommandsValues["UPD_FILE"] = Alter_file_current_backup;

	m_CommandsValues["SYNC_SND"] = Send_full_backup;
	m_CommandsValues["LOGOUT__"] = Logout;
	///* DEBUG*/cout << "m_CommandsValues contiene " << m_CommandsValues.size() << " elementi." << endl;
}

int main(int argc, char** argv) {

	int port, pid;
	int optval = 1;
	socklen_t optlen = sizeof(optval);

	struct sockaddr_in saddr, claddr;
	socklen_t claddr_len = sizeof(struct sockaddr_in);
	int s, s_c; //Socket
	MD5 md5;

	// Inizializzazione tabella comandi
	Initialize();

	if (argc != 2) {
		cerr << "- Errore nei parametri\n- Formato corretto: <porta>\n" << endl;
		return -1;
	}

	port = atoi(argv[1]);

	// Creazione socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		cerr << "- Errore nella crezione del socket, chiusura programma" << endl;
		return -1;
	}

	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);

	// Impostazione di SO_KEEPALIVE
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
		cerr << "- Errore nell'impostazione delle opzioni del socket, chiusura programma" << endl;
		close(s);
		return -1;
	}
	///* DEBUG */cout << "SO_KEEPALIVE: " << (optval ? "ON" : "OFF") << endl;

	// Bind del socket e listen
	if (bind(s, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
		cerr << "- Errore nel binding del socket, chiusura programma" << endl;
		close(s);
		return -1;
	}
	if (listen(s, BKLOG) == -1) {
		cerr << "- Errore nell'operazione di ascolto sul socket, chiusura programma" << endl;
		close(s);
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
	char buffer[MAX_BUF_LEN + 1] = "", comm[2 * COMM_LEN] = "";
	/*freopen(string("stdout_[" + to_string(pid) + "].log").c_str(), "w", stdout);
	 freopen(string("stderr_[" + to_string(pid) + "].log").c_str(), "w", stderr);*/

	// Prima operazione: login;
	/* DEBUG */strcpy(buffer, "Inserire dati utente o richiesta nuovo account");
	/* DEBUG */send(s_c, buffer, strlen(buffer), 0);
	if (!((ac = login(s_c)).is_complete())) {
		cout << "- [" << pid << "] Errore nella procedura di autenticazione" << endl;
		cout << "- [" << pid << "] Chiusura connessione" << endl;
		close(s_c);
		return;
	}

	while (ac.is_complete()) {
		// Attesa comando dal client
		cout << "- [" << pid << "] In attesa di comandi dal client" << endl;
		len = recv(s_c, comm, 2 * COMM_LEN, 0);
		if (len == COMM_LEN) {
			comm[len] = '\0';
			///* DEBUG */cout << comm << endl;
			///* DEBUG */cout << m_CommandsValues[comm] << endl;
			switch (m_CommandsValues[comm]) {
			case Select_folder:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'seleziona cartella'" << endl;
				cout << "- [" << pid << "] In attesa del percorso" << endl;
				send_command(s_c, "CMND_REC");
				f = select_folder(s_c, ac);
				if (!f.getPath().empty()) {
					cout << "- [" << pid << "] Cartella selezionata: " << f.getPath() << endl;
				} else {
					cout << "- [" << pid << "] Errore nella selezione della cartella" << endl;
				}
				break;
			case Clear_folder:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'deseleziona cartella'" << endl;
				send_command(s_c, "CMND_REC");
				f.clear_folder();
				cout << "- [" << pid << "] Cartella deselezionata" << endl;
				break;

			case Get_current_files:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'Stato corrente della cartella'" << endl;
				send_command(s_c, "CMND_REC");
				get_folder_stat(s_c, f);
				break;
			case New_file_current_backup:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'Nuovo file nel backup corrente'" << endl;
				send_command(s_c, "CMND_REC");
				new_file_backup(s_c, f);
				break;
			/* POSSIBILE RIDONDANZA ↓↑*/
			case Alter_file_current_backup:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'Modifica file nel bakcup corrente'" << endl;
				send_command(s_c, "CMND_REC");
				new_file_backup(s_c, f);
				break;
			///////////////////////////////////////////////////////////
			case Del_file_current_backup:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'Elimina file da backup corrente'" << endl;
				send_command(s_c, "CMND_REC");

				break;

			///////////////////////////////////////////////////////////
			case Receive_full_backup:
				cout << "- [" << pid << "] Ricevuto comando 'Backup cartella completo'" << endl;
				send_command(s_c, "CMND_REC");
				full_backup(s_c, f);
				break;
			case Send_full_backup:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'Invia backup completo'" << endl;
				send_command(s_c, "CMND_REC");

				break;
			case Logout:
				comm[0] = '\0';
				cout << "- [" << pid << "] Ricevuto comando 'logout'" << endl;
				send_command(s_c, "CMND_REC");
				ac.clear();
				return;
				break;
			default:
				if (ac.is_complete()) {
					cout << "- [" << pid << "] Ricevuto comando sconosciuto" << endl;
					cout << "- [" << pid << "] Chiusura connessione con utente " << ac.getUser() << endl;
					ac.clear();
				}
				close(s_c);
				return;
			}
		} else {
			cout << "- [" << pid << "] Errore nella ricezione" << endl;
			cout << "- [" << pid << "] Chiusura connessione" << endl;
			ac.clear();
			close(s_c);
			return;
		}
	}
}

void send_command(int s_c, const char *command) {
	//char buffer[COMM_LEN + 1];
	//strcpy(buffer, command);
	send(s_c, command, COMM_LEN, 0);
}

