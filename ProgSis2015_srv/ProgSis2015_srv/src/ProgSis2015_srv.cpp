//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================
#include "GenIncludes.hpp"

#include <map>
#include <set>
#include <semaphore.h>
#include <csignal>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BKLOG 10


using namespace std;

// Enum contente i possibili comandi al server, per uno switch più leggibile
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
int glob_sock = -1;
sem_t *sem_usertable = NULL;

/* SEGNALI */
// Gestore segnale di terminazione figlio
void child_handler(int param) {

  pid_t pid;
  while ((pid = waitpid((pid_t) (-1), NULL, WNOHANG)) > 0) {
    Logger::write_to_log("Il processo figlio [" + to_string(pid) + "] è terminato", DEBUG);
  }
  signal(SIGCHLD, child_handler);
}
// Gestore segnale di terminazione processo
void term_handler(int param) {

  Logger::write_to_log("Terminazione del processo, chiusura socket");
  close(glob_sock);
  exit(EXIT_FAILURE);
}
/* FINE SEGNALI */

void server_function(int, int);
void server_management();
void user_list();
void table_listing();
void user_removal();
void server_wipe();

// Inizializzazione della mappa dei comandi
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

  /* DEBUG
   * test_acc();
   * return 0;
  */

  Logger log("[" + to_string(getpid()) + "] debug.log");
  // Inizializzazione tabella comandi
  Initialize();



  if (argc != 2) {
    Logger::write_to_log("Errore nei parametri, formato corretto: <nome eseguibile> <porta> o <nome eseguibile> -m", ERROR);
    exit(EXIT_FAILURE);
  }

  if (!strcmp(argv[1], "-m"))
    server_management();
  else
    port = atoi(argv[1]);
  // Creazione socket
  if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    Logger::write_to_log("Errore nella creazione del socket, chiusura programma", ERROR);
    exit(EXIT_FAILURE);
  }
  Logger::write_to_log("Socket creato correttamente", DEBUG, LOG_ONLY);

  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  // Impostazione di SO_KEEPALIVE
  if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
    Logger::write_to_log("Errore nell'impostazione delle opzioni del socket, chiusura programma", ERROR);
    close(s);
    exit(EXIT_FAILURE);
  }
  Logger::write_to_log("Impostazione SO_KEEPALIVE del socket applicata correttamente", DEBUG, LOG_ONLY);
  // Bind del socket e listen
  if (bind(s, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
    Logger::write_to_log("Errore nel binding del socket, chiusura programma", ERROR);
    close(s);
    exit(EXIT_FAILURE);
  }
  Logger::write_to_log("Bind del socket effettuato correttamente", DEBUG, LOG_ONLY);
  if (listen(s, BKLOG) == -1) {
    Logger::write_to_log("Errore nell'operazione di ascolto sul socket, chiusura programma", ERROR);
    close(s);
    exit(EXIT_FAILURE);
  }
  Logger::write_to_log("Socket posto correttamente in ascolto", DEBUG, LOG_ONLY);
  // Area mappata:
  //   MMAP_SIZE -> MMAP_LINE_SIZE (Lunghezza massima nome utente, in byte) x MMAP_MAX_USER (Possibili utenti connessi contemporaneamente OvK)
  // + byte necessari per allocare il semaforo (sizeof(sem_t))
  if ((sem_usertable = (sem_t*) mmap(NULL, sizeof(sem_t) + MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0)) == MAP_FAILED) {
    Logger::write_to_log("Errore nella creazione dell'area di memoria condivisa, chiusura programma", ERROR);
    exit(EXIT_FAILURE);
  }
  usertable = (char*) sem_usertable + sizeof(sem_t);
  Logger::write_to_log("Area di memoria condivisa (tabella utenti e semaforo) creata correttamente, dimensione totale: " +  to_string(sizeof(sem_t) + MMAP_SIZE), DEBUG, LOG_ONLY);
  // Inizializzazione semaforo, non locale al processo, 1 accesso per volta
  if ((sem_init(sem_usertable, 1, 1)) == -1) {
    Logger::write_to_log("Errore nell'inizializzazione del semaforo", ERROR);
  }
  Logger::write_to_log("Semaforo inizializzato correttamente", DEBUG, LOG_ONLY);

  if ((signal(SIGCHLD, child_handler)) == SIG_ERR) {
    Logger::write_to_log("Errore nell'istanziamento del gestore del segnale di terminazione dei processi figli", ERROR);
  }
  Logger::write_to_log("Gestore del segnale di terminazione dei processi figli istanziato correttamente", DEBUG, LOG_ONLY);
  glob_sock = s;
  if (((signal(SIGINT, term_handler)) == SIG_ERR) || (signal(SIGTERM, term_handler) == SIG_ERR)) {
    Logger::write_to_log("Errore nell'istanziamento del gestore del segnale di terminazione del processo principale", ERROR);
  }
  Logger::write_to_log("Gestore del segnale di terminazione del processo principale istanziato correttamente", DEBUG, LOG_ONLY);
  for (;;) {
    /* Padre: in attesa di connessioni */
    Logger::write_to_log("In attesa di connessioni");
    s_c = accept(s, (struct sockaddr *) &claddr, &claddr_len);
    pid = fork();
    if (pid == -1) {
      Logger::write_to_log("Errore nella funzione fork()", ERROR);
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      // Processo figlio
      Logger::reopen_log("[" + to_string(getpid()) + "] debug.log");
      Logger::write_to_log("Connesso all'host: " + string(inet_ntoa(claddr.sin_addr)), DEBUG, LOG_ONLY);
      close(s);
      server_function(s_c, getpid());
      close(s_c);
      exit(EXIT_SUCCESS);
    } else {
      // Processo padre
      Logger::write_to_log("Il processo [" + to_string(pid) + "] è connesso all'host: " + string(inet_ntoa(claddr.sin_addr)));
      close(s_c);
    }
  }
  close(s);
  exit(EXIT_SUCCESS);
}

// Funzione di gestione operazioni del server
void server_function(int s_c, int pid) {

  set<string> used_tables;
  Account ac;
  Folder f;
  int len;
  char comm[COMM_LEN + 1] = "";
  // Try/catch per perdita connessione
  try {
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
            used_tables.insert(f.getTableName());
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
    Logger::write_to_log("Pulizia database e cartella file ricevuti in corso");
    db_maintenance(table);
  }
}

// Invio di un comando all'host
void send_command(int s_c, const char *command) {
  send(s_c, command, COMM_LEN, 0);
}

/* GESTIONE USERTABLE */
// Aggiunta di un utente alla lista degli utenti attualmente connessi
bool add_to_usertable(string user) {

  // Aggiungere un utente connesso all'elenco degli utenti:
  string line;
  // Si scorre l'area di memoria una linea per volta (numero di bytes pari a MMAP_LINE_SIZE)
  sem_wait(sem_usertable);
  for (int i = 0; i < MMAP_SIZE; i += MMAP_LINE_SIZE) {
    // Lettura fino al '\0', al massimo MMAP_LINE_SIZE-1 bytes (lunghezza username limitata a MMAP_LINE_SIZE-1 bytes)
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
// Rimozione di un utente dalla lista degli utenti attualmente connessi
void remove_from_usertable(string user) {

  // Eliminare un utente dall'elenco utenti:
  vector<string> users;
  string line;
  char mod = 0;
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
      mod = 1;
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
  if (mod == 1)
    Logger::write_to_log("Elenco di utenti connessi aggiornato, utente " + user + " rimosso");
  else
    Logger::write_to_log("Nessuna modifica all'elenco di utenti connessi", DEBUG, LOG_ONLY);
}
/* FINE GESTIONE USERTABLE */

/* FUNZIONI MANUTENZIONE SERVER */
// Funzione principale di manutenzione del server
void server_management() {

  char comm = 'c', comm_reset = 'n';
  char exit_flag = 0;
  cout << "Comandi disponibili:" << endl << "- Elenco [U]tenti" << endl << "- Elenco [T]abelle nel database" << endl << "- [E]limina utente" << endl << "- [A]zzera contenuto server" << endl
      << "- Us[C]ita" << endl;
  for (; exit_flag == 0;) {
    cout << "Comando: ";
    cin >> comm;
    switch (comm) {
      // Elenco utenti
      case 'u':
      case 'U':
        user_list();
        break;
      // Elenco tabelle, non di servizio, nel DB
      case 't':
      case 'T':
        table_listing();
        break;
      // Rimozione di un utente e relativi dati
      case 'e':
      case 'E':
        user_removal();
        break;
      // Eliminazione di tutti i dati dal server
      case 'a':
      case 'A':
        // Azione drastica, richiesta conferma
        cout << "VUOI DAVVERO ELIMINARE COMPLETAMENTE I DATI CONTENUTI NEL SERVER? S/[N]: ";
        cin >> comm_reset;
        switch (comm_reset) {
          case 's':
          case 'S':
            server_wipe();
            break;
          default:
            cout << "Annullamento" << endl;
        }
        break;
      // Uscita dalla manutenzione del server
      case 'c':
      case 'C':
        exit_flag = 1;
        cout << "Uscita" << endl;
        break;
      // Ricezione di un comando sconosciuto
      default:
        cout << "Comando sconosciuto: " << comm << endl;
        break;
    }
  }
  exit(EXIT_SUCCESS);
}
// Elenco utenti
void user_list() {

  try {
    SQLite::Database db("database.db3");
    string _query = "SELECT username FROM users ORDER BY user_id;";
    SQLite::Statement query(db, _query);
    bool r = query.executeStep();
    if (r) {
      cout << "■ Elenco utenti, per ordine di iscrizione: " << endl;
    } else {
      cout << "■ Nessun utente presente " << endl;
    }
    for (; r != false; r = query.executeStep()) {
      cout << query.getColumn(0) << endl;
    }
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nell'accesso al DB: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
// Elenco tabelle relative al server
void table_listing() {

  try {
    SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
    string _query = "SELECT name FROM sqlite_master WHERE type='table' AND name <> 'sqlite_sequence' ORDER BY name;";
    SQLite::Statement query(db, _query);
    cout << "■ Elenco tabelle presenti nel database: " << endl;
    for (bool r = query.executeStep(); r != false; r = query.executeStep()) {
      cout << query.getColumn(0) << endl;
    }
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nel Database: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
// Rimozione utente e relative cartelle
void user_removal() {
  // Eliminazione utente e relativi contenuti:
  // - Si richiede l'utente da eliminare
  // - Si elimina l'entry dell'utente da 'users'
  // - Si individuano le tabelle dell'utente e si eliminano;
  // -- Si legge l'elenco delle tabelle
  // -- Si individuano quelle che contengono il nome all'utente cercato
  // -- Si eliminano le tabelle trovate
  // - Si individuano le cartelle dell'utente e si eliminano
  string user_to_delete;
  vector<string> tables_list;
  vector<string> tables_to_delete;
  // Richiesta utente da eliminare
  cout << "Utente da eliminare? ";
  cin.clear();
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  getline(cin, user_to_delete);
  try {
    SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
    // Eliminazione entry relativa all'utente dalla tabella 'users'
    if (SQLite::Statement(db, "DELETE FROM 'users' WHERE username='" + user_to_delete + "';").exec() == 1) {
      cout << "Utente " << user_to_delete << " rimosso dalla tabella utenti" << endl;
    } else {
      cout << "Utente " << user_to_delete << " non trovato, uscita" << endl;
      return;
    }
    // Ricerca tabelle relative all'utente da eliminare
    string _query = "SELECT name FROM sqlite_master WHERE type='table' AND name <> 'sqlite_sequence' ORDER BY name;";
    SQLite::Statement query(db, _query);
    // Costruzione lista completa tabelle, eccetto 'sqlite_sequence' in quanto tabella tabella di servizio del DB
    for (bool r = query.executeStep(); r != false; r = query.executeStep()) {
      tables_list.push_back(query.getColumn(0));
    }
    // Costruzione lista tabelle relative all'utente considerato, identificate dalla presenza di _[nome_utente]_
    for (string t : tables_list) {
      if (t.find(string("_" + user_to_delete + "_")) != string::npos) {
        tables_to_delete.push_back(t);
      }
    }
    // Ciclo con doppia funzione:
    for (string t_e : tables_to_delete) {
      // eliminazione dei file dell'utente...
      SQLite::Statement query(db, "SELECT DISTINCT File_SRV FROM '" + t_e + "';");
      while (query.executeStep()) {
        remove(query.getColumn("File_SRV"));
      }
      // ...ed eliminazione tabella (DROP)
      SQLite::Statement(db, "DROP TABLE '" + t_e + "';").exec();
    }
    // Eliminazione cartelle vuote rimaste
    system("find ./ReceivedFiles/* -empty -type d -delete 2> /dev/null");
    cout << "Rimossi tutti i contenuti dell'utente " + user_to_delete + " dal database" << endl;
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nell'accesso al DB: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
// Eliminazione contenuto del server
void server_wipe() {
  // Azzeramento contenuto del server:
  // - Si eliminano tutte le tabelle eccetto 'users'
  // - Si eliminano tutte le entry da 'users'
  // - Si eliminano interamente il contenuto dalla cartella ReceivedFiles (eccetto la cartella stessa)
  try {
    vector<string> to_delete_list;
    SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
    // Eliminazione tabelle eccetto 'users'
    string _query = "SELECT name FROM sqlite_master WHERE type='table' AND name <> 'sqlite_sequence' AND name <> 'users' ORDER BY name;";
    SQLite::Statement query(db, _query);
    for (bool r = query.executeStep(); r != false; r = query.executeStep()) {
      to_delete_list.push_back(query.getColumn(0));
    }
    for (string t : to_delete_list) {
      SQLite::Statement(db, "DROP TABLE '" + t + "';").exec();
    }
    cout << "Tabelle delle cartelle eliminate" << endl;
    // Eliminazione entry da 'users'
    SQLite::Statement(db, "DELETE FROM 'users'").exec();
    cout << "Tabella 'users' azzerata" << endl;
    // Eliminazione sotto-cartelle di ReceivedFiles
    system("rm -rf ./ReceivedFiles/*");
    cout << "Svuotata cartella con i file ricevuti" << endl;
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nel Database: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
/*FINE FUNZIONI MANUTENZIONE SERVER */
