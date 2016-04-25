//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================
#include "GenIncludes.hpp"

#include <map>
#include <semaphore.h>
#include <csignal>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BKLOG 10
#define FILE_NAME_LEN 30
#define MMAP_SIZE 4096
#define MMAP_LINE_SIZE 32
#define MMAP_MAX_USER 128

using namespace std;

enum StringValue {
	Undefined,
	Select_folder,
	Clear_folder,
	Receive_file,
	Receive_full_backup,
	Send_full_backup,
	Send_single_file,
	New_file_current_backup,
	Del_file_current_backup,
	Alter_file_current_backup,
	Get_current_folder_status,
	Logout
};

static map<string, StringValue> m_CommandsValues;
// Puntatore per mmap
char *usertable = NULL;
sem_t *sem_usertable = NULL;

void child_handler(int param) {

	pid_t pid;
	while ((pid = waitpid((pid_t) (-1), NULL, WNOHANG)) > 0) {
		Logger::write_to_log("Il processo figlio [" + to_string(pid) + "] è terminato", DEBUG, LOG_ONLY);
	}
	signal(SIGCHLD, child_handler);
}

void server_function(int, int);

static void Initialize() {

	m_CommandsValues["SET_FOLD"] = Select_folder;
	m_CommandsValues["CLR_FOLD"] = Clear_folder;
	m_CommandsValues["REC_FILE"] = Receive_file;
	m_CommandsValues["SYNC_RCV"] = Receive_full_backup;
	m_CommandsValues["FLD_STAT"] = Get_current_folder_status;
	m_CommandsValues["NEW_FILE"] = New_file_current_backup;
	m_CommandsValues["UPD_FILE"] = Alter_file_current_backup;
	m_CommandsValues["DEL_FILE"] = Del_file_current_backup;

	m_CommandsValues["SYNC_SND"] = Send_full_backup;
	m_CommandsValues["FILE_SND"] = Send_single_file;
	m_CommandsValues["LOGOUT__"] = Logout;
	Logger::write_to_log("Mappa dei comandi inizializzata correttamente, numero di elementi presenti: " + to_string(m_CommandsValues.size()), DEBUG, LOG_ONLY);
}

int main(int argc, char** argv) {

	int port, pid;
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	struct sockaddr_in saddr, claddr;
	socklen_t claddr_len = sizeof(struct sockaddr_in);
	int s, s_c; //Socket

	Logger log("[" + to_string(getpid()) + "] debug.log");
	// Inizializzazione tabella comandi
	Initialize();

	if (argc != 2) {
		Logger::write_to_log("Errore nei parametri, formato corretto: <nome eseguibile> <porta>", ERROR);
		return (-1);
	}

	port = atoi(argv[1]);
	// Creazione socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		Logger::write_to_log("Errore nella creazione del socket, chiusura programma", ERROR);
		return -1;
	}
	Logger::write_to_log("Socket creato correttamente", DEBUG, LOG_ONLY);

	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	// Impostazione di SO_KEEPALIVE
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
		Logger::write_to_log("Errore nell'impostazione delle opzioni del socket, chiusura programma", ERROR);
		close(s);
		return (-1);
	}
	Logger::write_to_log("Impostazione SO_KEEPALIVE del socket applicata correttamente", DEBUG, LOG_ONLY);
	// Bind del socket e listen
	if (bind(s, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
		Logger::write_to_log("Errore nel binding del socket, chiusura programma", ERROR);
		close(s);
		return (-1);
	}
	Logger::write_to_log("Bind del socket effettuato correttamente", DEBUG, LOG_ONLY);
	if (listen(s, BKLOG) == -1) {
		Logger::write_to_log("Errore nell'operazione di ascolto sul socket, chiusura programma", ERROR);
		close(s);
		return (-1);
	}
	Logger::write_to_log("Socket posto correttamente in ascolto", DEBUG, LOG_ONLY);
	// Area mappata:
	//   4096 Byte -> 32 (Lunghezza massima nome utente) x 128 (Possibili utenti connessi contemporaneamente OvK)
	// + byte necessari per allocare il semaforo (sizeof(sem_t))
	if ((sem_usertable = (sem_t*) mmap(NULL, sizeof(sem_t) + MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0)) == MAP_FAILED) {
		Logger::write_to_log("Errore nella creazione dell'area di memoria condivisa, chiusura programma", ERROR);
		return (-1);
	}
	usertable = (char*) sem_usertable + sizeof(sem_t);
	Logger::write_to_log("Area di memoria condivisa (tabella utenti e semaforo) creata correttamente", DEBUG, LOG_ONLY);
	// Inizializzazione semaforo, non locale al processo, 1 accesso per volta
	if ((sem_init(sem_usertable, 1, 1)) == -1) {
		Logger::write_to_log("Errore nell'inizializzazione del semaforo", ERROR);
	}
	Logger::write_to_log("Semaforo inizializzato correttamente", DEBUG, LOG_ONLY);

	if ((signal(SIGCHLD, child_handler)) == SIG_ERR) {
		Logger::write_to_log("Errore nell'istanziamento del gestore del segnale di terminazione dei processi figli", ERROR);
	}
	Logger::write_to_log("Gestore del segnale di terminazione dei processi figli istanziato correttamente", DEBUG, LOG_ONLY);
	for (;;) {
		/* Padre: in attesa di connessioni */
		Logger::write_to_log("In attesa di connessioni");
		s_c = accept(s, (struct sockaddr *) &claddr, &claddr_len);
		pid = fork();
		if (pid == -1) {
			Logger::write_to_log("Errore nella funzione fork()", ERROR);
			return (-1);
		} else if (pid == 0) {
			// Processo figlio
			Logger::reopen_log("[" + to_string(getpid()) + "] debug.log");
			Logger::write_to_log("Connesso all'host: " + string(inet_ntoa(claddr.sin_addr)), DEBUG, LOG_ONLY);
			close(s);
			server_function(s_c, getpid());
			close(s_c);
			return (0);
		} else {
			// Processo padre
			Logger::write_to_log("Il processo [" + to_string(pid) + "] è connesso all'host: " + string(inet_ntoa(claddr.sin_addr)));
			close(s_c);
		}
	}
	close(s);
	return (0);
}

// Funzione di gestione operazioni del server
void server_function(int s_c, int pid) {

	vector<string> used_tables;
//Logger log("[" + to_string(pid) + "] debug.log");
	Account ac;
	Folder f;
	int len;
	char comm[COMM_LEN + 1] = "";

	try {
		// Try/catch per perdita connessione
		// Prima operazione: login;
		if (!((ac = login(s_c, usertable)).is_complete())) {
			Logger::write_to_log("Errore nella procedura di autenticazione, chiusura connessione", ERROR);
			close(s_c);
			return;
		}
		while (ac.is_complete()) {
			// Attesa comando dal client
			Logger::write_to_log("In attesa di comandi dall'utente");
			len = recv(s_c, comm, COMM_LEN, 0);
			if ((len == 0) || (len == -1)) {
				// Ricevuta stringa vuota: connessione persa
				throw runtime_error("connessione persa");
			}
			comm[len] = '\0';
			switch (m_CommandsValues[comm]) {
			case Select_folder:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'seleziona cartella', in attesa del percorso");
				send_command(s_c, "CMND_REC");
				f = ac.select_folder(s_c);
				if (!f.getPath().empty()) {
					used_tables.push_back(f.getTableName());
					Logger::write_to_log("Cartella selezionata: " + f.getPath());
				} else {
					Logger::write_to_log("Errore nella selezione della cartella", ERROR);
				}
				break;
			case Clear_folder:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'deseleziona cartella'");
				send_command(s_c, "CMND_REC");
				f.clear_folder();
				Logger::write_to_log("Cartella deselezionata");
				break;
			case Get_current_folder_status:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'stato corrente della cartella'");
				send_command(s_c, "CMND_REC");
				f.get_folder_stat(s_c);
				break;
			case Receive_full_backup:
				Logger::write_to_log("Ricevuto comando 'backup cartella completo'");
				send_command(s_c, "CMND_REC");
				f.full_backup(s_c);
				break;
			case New_file_current_backup:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'nuovo file nel backup corrente'");
				send_command(s_c, "CMND_REC");
				f.new_file_backup(s_c);
				break;
			case Alter_file_current_backup:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'modifica file nel backup corrente'");
				send_command(s_c, "CMND_REC");
				f.new_file_backup(s_c);
				break;
			case Del_file_current_backup:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'elimina file da backup corrente'");
				send_command(s_c, "CMND_REC");
				f.delete_file_backup(s_c);
				break;
			case Send_full_backup:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'invia backup completo'");
				send_command(s_c, "CMND_REC");
				f.send_backup(s_c);
				break;
			case Send_single_file:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'invia singolo file dal backup'");
				send_command(s_c, "CMND_REC");
				f.send_single_file(s_c);
				break;
			case Logout:
				comm[0] = '\0';
				Logger::write_to_log("Ricevuto comando 'logout'");
				//cout << "- Ricevuto comando 'logout'" << endl;
				send_command(s_c, "CMND_REC");
				ac.clear();
				Logger::write_to_log("Disconnessione effettuata");
				break;
			default:
				throw runtime_error("ricevuto comando sconosciuto");
			}
		}
	} catch (runtime_error& e) {
		Logger::write_to_log("Disconnessione: " + string(e.what()), ERROR);
	}
	ac.clear();
	close(s_c);
	for (string table : used_tables) {
		db_maintenance(table);
	}
	return;
}

void send_command(int s_c, const char *command) {
	send(s_c, command, COMM_LEN, 0);
}

bool add_to_usertable(string user) {

// Aggiungere un utente connesso all'lenco degli utenti:
	string line;
// Si scorre l'area di memoria a 32 caratteri per volta
	sem_wait(sem_usertable);
	for (int i = 0; i < MMAP_SIZE; i += MMAP_LINE_SIZE) {
		// Lettura fino al '\0', al massimo 31 caratteri (lunghezza username limitata a 31 caratteri)
		line.assign((usertable + i));
		if (line.empty()) {
			// Se la riga è vuota -> si inserisce il nome dell'utente corrente
			sprintf(usertable + i, "%s", user.c_str());
			Logger::write_to_log("Elenco di utenti connessi aggiornato, utente " + user + " aggiunto");
			sem_post(sem_usertable);
			return true;
		} else if (!line.compare(user)) {
			// Se l'utente nella riga corrisponde a quello che sta tentando il login
			Logger::write_to_log("L'account risulta già connesso", ERROR);
			sem_post(sem_usertable);
			return false;
		}
		// La riga non è vuota e l'utente nella riga corrente NON corrisponde a quello che sta tentando di fare login
	}
// Se si è raggiunta la fine (spazio esaurito per nuovi utenti), si ritorna false
	Logger::write_to_log("È stato raggiunto il numero massimo di utenti connessi", ERROR);
	sem_post(sem_usertable);
	return false;

}

void remove_from_usertable(string user) {

// Eliminare un utente dall'elenco utenti:
	vector<string> users;
	string line;
// Si scorre l'elenco degli utenti
	sem_wait(sem_usertable);
	for (int i = 0; i < MMAP_SIZE; i += MMAP_LINE_SIZE) {
		// Si legge la riga corrente...
		line.assign(usertable + i);
		// ...e la si azzera
		usertable[i] = '\0';
		if (line.empty()) {
			// Se la linea corrente è vuota: si termina il ciclo
			break;
		} else if (!line.compare(user)) {
			//  Se la linea attuale contiene l'utente da eliminare, si prosegue con l'iterazione successiva
			continue;
		}
		// Altrimenti si salva l'utente corrente nel vettore di stringhe
		users.push_back(line);
	}
// - Si ripopola l'area mappata con il nuovo elenco di nomi utente
	int i = 0;
	for (string s : users) {
		sprintf(usertable + i, "%s", user.c_str());
		i += MMAP_LINE_SIZE;
	}
	sem_post(sem_usertable);
	Logger::write_to_log("Elenco di utenti connessi aggiornato, utente " + user + " rimosso");
}
