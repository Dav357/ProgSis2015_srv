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

const int BKLOG = 10;

using namespace std;

// Enum contente i possibili comandi al server, per uno switch più leggibile
enum stringValue {
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

static map<string, stringValue> m_CommandsValues;
// Semaforo per accesso ad area di memoria condivisa
sem_t *sem_usertable = NULL;
// Puntatore per mmap
char *usertable = NULL;

/* SEGNALI */
// Gestore segnale di terminazione figlio
void childHandler(int param) {

  pid_t pid;
  while ((pid = waitpid((pid_t) (-1), NULL, WNOHANG)) > 0) {
    Logger::writeToLog("Il processo figlio [" + to_string(pid) + "] è terminato", DEBUG);
  }
  signal(SIGCHLD, childHandler);
}
// Gestore segnale di terminazione processo
void termHandler(int param) {

  throw runtime_error("ricevuto segnale di terminazione");
}
/* FINE SEGNALI */

/* FUNZIONI DI MANUTENZIONE DEL SERVER */
void serverFunction(int, int);
void serverManagement();
void createConfigFile();
void createNewDBFile();
void userList();
void tableListing();
void userRemoval();
void serverWipe();
/* FINE FUNZIONI DI MANUTENZIONE DEL SERVER */

// Inizializzazione delle impostazioni del server
static void initializeServerSettings(string config_file) {
  // Lettura file di configurazione, se presente deve essere completo, altrimenti si carica la configurazione di default
  ifstream conf_file(config_file);
  string files_folder, DB_file;
  int maxUsernameLen, maxUsers;

  // Prima linea: cartella per i file
  conf_file >> files_folder;
  // Seconda linea: file del database
  conf_file >> DB_file;
  // Terza linea: lunghezza massima nome utente
  conf_file >> maxUsernameLen;
  // Quarta linea: numero massimo di utenti
  conf_file >> maxUsers;

  if (!files_folder.empty() && !DB_file.empty() && maxUsernameLen != 0 && maxUsers != 0) {
    ServerSettings s(files_folder, DB_file, maxUsers, maxUsernameLen);
    Logger::writeToLog("Configurazione caricata correttamente", DEBUG, LOG_ONLY);
  } else {
    Logger::writeToLog("Errore nella lettura della configurazione, utilizzo parametri di default", ERROR, LOG_ONLY);
  }
  Logger::writeToLog(
      "Cartella di salvataggio: " + ServerSettings::getFilesFolder() + ", file di database: " + ServerSettings::getDBFile() + ", limite bytes username: "
          + to_string(ServerSettings::getMaxUsernameLen()) + ", limite utenti contemporanei: " + to_string(ServerSettings::getMaxUsers()), DEBUG, LOG_ONLY);
}

// Inizializzazione della mappa dei comandi
static void initialize() {

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
  Logger::writeToLog("Mappa dei comandi inizializzata correttamente, numero di elementi presenti: " + to_string(m_CommandsValues.size()), DEBUG, LOG_ONLY);
}

int main(int argc, char** argv) {

  int port = 0, pid, optval = 1;
  bool manag = false;
  string config_file_name;
  socklen_t optlen = sizeof(optval);
  struct sockaddr_in saddr, claddr;
  socklen_t claddr_len = sizeof(struct sockaddr_in);
  int s, s_c; //Socket
  ServerSettings base_set;

  Logger log("[" + to_string(getpid()) + "] debug.log");

  if (argc != 5 && argc != 4) {
    Logger::writeToLog("Errore nei parametri, formati accettati:\n -- " + string(argv[0]) + " -f <file di configurazione> -p <porta>\n -- " + string(argv[0]) + " -f <file di configurazione> -m",
        ERROR);
    return (EXIT_FAILURE);
  }

  for (int i = 1; i < argc; i += 2) {
    if (!strcmp(argv[i], "-f")) {
      config_file_name.assign(argv[i + 1]);
    } else if (!strcmp(argv[i], "-p")) {
      port = atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "-m")) {
      manag = true;
      port = -1;
    } else {
      Logger::writeToLog("Errore nei parametri, flag sconosciuto: " + string(argv[i]), ERROR);
      return (EXIT_FAILURE);
    }
  }
  if (port == 0) {
    Logger::writeToLog("Errore nei parametri, porta non assegnata", ERROR);
    return (EXIT_FAILURE);
  } else if (config_file_name.empty()) {
    Logger::writeToLog("Errore nei parametri, file di configurazione non assegnato", ERROR);
    return (EXIT_FAILURE);
  }
  if (manag) {
    serverManagement();
  }

  /* INIZIALIZZAZIONI */
  // Inizializzazione tabella comandi
  initialize();
  // Inizializzazione server da file ricevuto
  initializeServerSettings(config_file_name);
  /* FINE INIZIALIZZAZIONI */

  // Creazione socket
  if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    Logger::writeToLog("Errore nella creazione del socket, chiusura programma", ERROR);
    return (EXIT_FAILURE);
  }
  Logger::writeToLog("Socket creato correttamente", DEBUG, LOG_ONLY);

  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  // Impostazione di SO_KEEPALIVE
  if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
    Logger::writeToLog("Errore nell'impostazione delle opzioni del socket, chiusura programma", ERROR);
    close(s);
    return (EXIT_FAILURE);
  }
  Logger::writeToLog("Impostazione SO_KEEPALIVE del socket applicata correttamente", DEBUG, LOG_ONLY);
  // Bind del socket e listen
  if (bind(s, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
    Logger::writeToLog("Errore nel binding del socket, chiusura programma", ERROR);
    close(s);
    return (EXIT_FAILURE);
  }
  Logger::writeToLog("Bind del socket effettuato correttamente", DEBUG, LOG_ONLY);
  if (listen(s, BKLOG) == -1) {
    Logger::writeToLog("Errore nell'operazione di ascolto sul socket, chiusura programma", ERROR);
    close(s);
    return (EXIT_FAILURE);
  }
  Logger::writeToLog("Socket posto correttamente in ascolto", DEBUG, LOG_ONLY);
  // Area mappata:
  //   MMAP_SIZE -> MMAP_LINE_SIZE (Lunghezza massima nome utente, in byte) x MMAP_MAX_USER (Possibili utenti connessi contemporaneamente OvK)
  // + byte necessari per allocare il semaforo (sizeof(sem_t))
  if ((sem_usertable = (sem_t*) mmap(NULL, sizeof(sem_t) + ServerSettings::getMMapSize(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0)) == MAP_FAILED) {
    Logger::writeToLog("Errore nella creazione dell'area di memoria condivisa, chiusura programma", ERROR);
    return (EXIT_FAILURE);
  }
  usertable = (char*) sem_usertable + sizeof(sem_t);
  Logger::writeToLog("Area di memoria condivisa (tabella utenti e semaforo) creata correttamente, dimensione totale: " + to_string(sizeof(sem_t) + ServerSettings::getMMapSize()), DEBUG, LOG_ONLY);
  // Inizializzazione semaforo, non locale al processo, 1 accesso per volta
  if ((sem_init(sem_usertable, 1, 1)) == -1) {
    Logger::writeToLog("Errore nell'inizializzazione del semaforo", ERROR);
  }
  Logger::writeToLog("Semaforo inizializzato correttamente", DEBUG, LOG_ONLY);

  if ((signal(SIGCHLD, childHandler)) == SIG_ERR) {
    Logger::writeToLog("Errore nell'istanziamento del gestore del segnale di terminazione dei processi figli", ERROR);
  }
  Logger::writeToLog("Gestore del segnale di terminazione dei processi figli istanziato correttamente", DEBUG, LOG_ONLY);
  if (((signal(SIGINT, termHandler)) == SIG_ERR) || (signal(SIGTERM, termHandler) == SIG_ERR)) {
    Logger::writeToLog("Errore nell'istanziamento del gestore del segnale di terminazione del processo principale", ERROR);
  }
  Logger::writeToLog("Gestore del segnale di terminazione del processo principale istanziato correttamente", DEBUG, LOG_ONLY);

  try {
    for (;;) {
      /* Padre: in attesa di connessioni */
      Logger::writeToLog("In attesa di connessioni");
      s_c = accept(s, (struct sockaddr *) &claddr, &claddr_len);
      pid = fork();
      if (pid == -1) {
        Logger::writeToLog("Errore nella funzione fork()", ERROR);
        exit(EXIT_FAILURE);
      } else if (pid == 0) {
        // Processo figlio
        Logger::reopenLog("[" + to_string(getpid()) + "] debug.child.log");
        Logger::writeToLog("Connesso all'host: " + string(inet_ntoa(claddr.sin_addr)), DEBUG, LOG_ONLY);
        close(s);
        serverFunction(s_c, getpid());
        close(s_c);
        exit(EXIT_SUCCESS);
      } else {
        // Processo padre
        Logger::writeToLog("Il processo [" + to_string(pid) + "] è connesso all'host: " + string(inet_ntoa(claddr.sin_addr)));
        close(s_c);
      }
    }
  } catch (runtime_error& e) {
    Logger::writeToLog("Chiusura programma: " + string(e.what()));
  }
  close(s);
  return (EXIT_SUCCESS);
}

// Funzione di gestione operazioni del server
void serverFunction(int s_c, int pid) {

  set<string> usedTables;
  Account ac;
  Folder f;
  int len;
  char comm[COMM_LEN + 1] = "";
// Try/catch per perdita connessione
  try {
    // Prima operazione: login;
    if (!((ac = login(s_c, usertable)).isComplete())) {
      Logger::writeToLog("Errore nella procedura di autenticazione, chiusura connessione", ERROR);
      close(s_c);
      return;
    }
    while (ac.isComplete()) {
      // Attesa comando dal client
      Logger::writeToLog("In attesa di comandi dall'utente");
      len = recv(s_c, comm, COMM_LEN, 0);
      if ((len == 0) || (len == -1)) {
        // Ricevuta stringa vuota: connessione persa
        throw runtime_error("connessione persa");
      }
      comm[len] = '\0';
      switch (m_CommandsValues[comm]) {
        case Select_folder:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'seleziona cartella', in attesa del percorso");
          sendCommand(s_c, "CMND_REC");
          f = ac.selectFolder(s_c);
          if (!f.getPath().empty()) {
            usedTables.insert(f.getTableName());
            Logger::writeToLog("Cartella selezionata: " + f.getPath());
          } else {
            Logger::writeToLog("Errore nella selezione della cartella", ERROR);
          }
          break;
        case Clear_folder:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'deseleziona cartella'");
          sendCommand(s_c, "CMND_REC");
          f.clearFolder();
          Logger::writeToLog("Cartella deselezionata");
          break;
        case Get_current_folder_status:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'stato corrente della cartella'");
          sendCommand(s_c, "CMND_REC");
          f.getFolderStat(s_c);
          break;
        case Receive_full_backup:
          Logger::writeToLog("Ricevuto comando 'backup cartella completo'");
          sendCommand(s_c, "CMND_REC");
          f.fullBackup(s_c);
          break;
        case New_file_current_backup:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'nuovo file nel backup corrente'");
          sendCommand(s_c, "CMND_REC");
          f.newFileBackup(s_c);
          break;
        case Alter_file_current_backup:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'modifica file nel backup corrente'");
          sendCommand(s_c, "CMND_REC");
          f.newFileBackup(s_c);
          break;
        case Del_file_current_backup:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'elimina file da backup corrente'");
          sendCommand(s_c, "CMND_REC");
          f.deleteFileBackup(s_c);
          break;
        case Send_full_backup:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'invia backup completo'");
          sendCommand(s_c, "CMND_REC");
          f.sendBackup(s_c);
          break;
        case Send_single_file:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'invia singolo file dal backup'");
          sendCommand(s_c, "CMND_REC");
          f.sendSingleFile(s_c);
          break;
        case Logout:
          comm[0] = '\0';
          Logger::writeToLog("Ricevuto comando 'logout'");
          sendCommand(s_c, "CMND_REC");
          ac.clear();
          Logger::writeToLog("Disconnessione effettuata");
          break;
        default:
          throw runtime_error("ricevuto comando sconosciuto");
      }
    }
  } catch (runtime_error& e) {
    Logger::writeToLog("Disconnessione: " + string(e.what()), ERROR);
  }
  ac.clear();
  close(s_c);
  for (string table : usedTables) {
    Logger::writeToLog("Pulizia database e cartella file ricevuti in corso");
    dbMaintenance(table);
  }
}

// Invio di un comando all'host
void sendCommand(int s_c, const char *command) {
  send(s_c, command, COMM_LEN, 0);
}

/* GESTIONE USERTABLE */
// Aggiunta di un utente alla lista degli utenti attualmente connessi
bool addToUsertable(string user) {

  // Aggiungere un utente connesso all'elenco degli utenti:
  string line;
  // Si scorre l'area di memoria una linea per volta (numero di bytes pari a MMAP_LINE_SIZE)
  sem_wait(sem_usertable);
  for (int i = 0; i < ServerSettings::getMMapSize(); i += ServerSettings::getMMapMaxLineSize()) {
    // Lettura fino al '\0', al massimo MMAP_LINE_SIZE-1 bytes (lunghezza username limitata a MMAP_LINE_SIZE-1 bytes)
    line.assign((usertable + i));
    if (line.empty()) {
      // Se la riga è vuota -> si inserisce il nome dell'utente corrente
      sprintf(usertable + i, "%s", user.c_str());
      Logger::writeToLog("Elenco di utenti connessi aggiornato, utente " + user + " aggiunto");
      sem_post(sem_usertable);
      return true;
    } else if (!line.compare(user)) {
      // Se l'utente nella riga corrisponde a quello che sta tentando il login
      Logger::writeToLog("L'account risulta già connesso", ERROR);
      sem_post(sem_usertable);
      return false;
    }
    // La riga non è vuota e l'utente nella riga corrente NON corrisponde a quello che sta tentando di fare login
  }
  // Se si è raggiunta la fine (spazio esaurito per nuovi utenti), si ritorna false
  Logger::writeToLog("È stato raggiunto il numero massimo di utenti connessi", ERROR);
  sem_post(sem_usertable);
  return false;

}
// Rimozione di un utente dalla lista degli utenti attualmente connessi
void removeFromUsertable(string user) {

// Eliminare un utente dall'elenco utenti:
  vector<string> users;
  string line;
  char mod = 0;
// Si scorre l'elenco degli utenti
  sem_wait(sem_usertable);
  for (int i = 0; i < ServerSettings::getMMapSize(); i += ServerSettings::getMMapMaxLineSize()) {
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
    i += ServerSettings::getMMapMaxLineSize();
  }
  sem_post(sem_usertable);
  if (mod == 1)
    Logger::writeToLog("Elenco di utenti connessi aggiornato, utente " + user + " rimosso");
  else
    Logger::writeToLog("Nessuna modifica all'elenco di utenti connessi", DEBUG, LOG_ONLY);
}
/* FINE GESTIONE USERTABLE */

/* FUNZIONI MANUTENZIONE SERVER */
// Funzione principale di manutenzione del server
void serverManagement() {

  char comm = 'c', comm_reset = 'n';
  char exit_flag = 0;
  cout << "Comandi disponibili:" << endl << "- Crea [F]ile di configurazione" << endl << "- Crea [D]atabase" << endl << "- Elenco [U]tenti" << endl << "- Elenco [T]abelle nel database" << endl
      << "- [E]limina utente" << endl << "- [A]zzera contenuto server" << endl << "- Us[C]ita" << endl;
  for (; exit_flag == 0;) {
    cout << "Comando: ";
    cin >> comm;
    switch (comm) {
      // Scrivi file di configurazione
      case 'f':
      case 'F':
        createConfigFile();
        break;
      case 'd':
      case 'D':
        // Crea nuovo DB, con la sola cartella utenti vuota
        createNewDBFile();
        break;
        // Elenco utenti
      case 'u':
      case 'U':
        userList();
        break;
        // Elenco tabelle, non di servizio, nel DB
      case 't':
      case 'T':
        tableListing();
        break;
        // Rimozione di un utente e relativi dati
      case 'e':
      case 'E':
        userRemoval();
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
            serverWipe();
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
// Scrittura file di configurazione
void createConfigFile() {
  // Si richiede:
  // - Nome file
  // -- Cartella file ricevuti
  // -- Nome database
  // -- Lunghezza massima nome utente
  // -- Numero massimo utenti connessi contemporaneamente
  string buffer;
  cout << "Nome del file da creare? ";
  cin.clear();
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  getline(cin, buffer);
  ofstream new_config_file(buffer);
  cout << "Cartella da usare per i file ricevuti? ";
  getline(cin, buffer);
  new_config_file << buffer << endl;
  cout << "File di database? ";
  getline(cin, buffer);
  new_config_file << buffer << endl;
  cout << "Lunghezza massima (in byte) username? ";
  getline(cin, buffer);
  new_config_file << buffer << endl;
  cout << "Numero massimo di utenti contemporanei? ";
  getline(cin, buffer);
  new_config_file << buffer << endl;
  new_config_file.close();
  cout << "File di configurazione creato e popolato" << endl;
}
// Creazione di un nuovo database, inizializzato con la sola tabella per gli utenti
void createNewDBFile() {
  string buffer;
  cout << "Nome del database da creare? ";
  cin.clear();
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  getline(cin, buffer);
  SQLite::Database db(buffer, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE);
  cout << "Nuovo database aperto";
  SQLite::Statement query(db, "CREATE TABLE users (user_id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, sha2_pass TEXT (64) NOT NULL);");
  query.exec();
  cout << ", tabella utenti inserita" << endl;
}
// Elenco utenti
void userList() {

  try {
    SQLite::Database db(ServerSettings::getDBFile());
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
void tableListing() {

  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
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
void userRemoval() {
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
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
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
    system(string("find ./" + ServerSettings::getFilesFolder() + "/* -empty -type d -delete 2> /dev/null").c_str());
    cout << "Rimossi tutti i contenuti dell'utente " + user_to_delete + " dal database" << endl;
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nell'accesso al DB: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
// Eliminazione contenuto del server
void serverWipe() {
// Azzeramento contenuto del server:
// - Si eliminano tutte le tabelle eccetto 'users'
// - Si eliminano tutte le entry da 'users'
// - Si elimina interamente il contenuto dalla cartella dei file ricevuti (eccetto la cartella stessa)
  try {
    vector<string> to_delete_list;
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
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
    // Eliminazione sotto-cartelle contenti i file degli utenti
    system(string("rm -rf ./" + ServerSettings::getFilesFolder() + "/*").c_str());
    cout << "Svuotata cartella con i file ricevuti" << endl;
  } catch (SQLite::Exception& e) {
    cerr << "❌ Errore nel Database: " << e.what() << endl << "❌ Operazione non eseguita." << endl;
  }
}
/*FINE FUNZIONI MANUTENZIONE SERVER */
