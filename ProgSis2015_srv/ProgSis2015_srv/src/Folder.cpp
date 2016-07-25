/*
 * Folder.cpp
 *
 *  Created on: 05 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include "Folder.h"
using namespace std;

// Costruttore
Folder::Folder(string p, Account& user) :
        user(user) {

  string tmp("table_" + user.getUser() + "_" + p);
  path = p;
  table_name = tmp;
}

// Azzera le informazioni relative alla cartella
void Folder::clearFolder() {
  path = "";
  table_name = "";
  files.clear();
}

// Inserisci il file specificato nella cartella
void Folder::insertFile(File f) {
  files.push_back(f);
}

// Crea nuova tabella nel DB relativa alla cartella
bool Folder::createTableFolder() {

  SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
  string _query(
      "CREATE TABLE [" + table_name
      + "] (File_ID INTEGER PRIMARY KEY AUTOINCREMENT, File_CL TEXT NOT NULL, Last_Modif INTEGER (8) NOT NULL, File_SRV TEXT NOT NULL,  Hash STRING (32), Versione_BCK INTEGER (2));");
  SQLite::Statement query(db, _query);

  if (query.exec() == 0) {
    Logger::writeToLog("Tabella " + table_name + " creata con successo");
    return true;
  } else {
    Logger::writeToLog("Errore nella creazione della tabella " + table_name, ERROR);
    return false;
  }
}

// Fornisci i dati relativi ad una specifica cartella
void Folder::getFolderStat(int s_c) {

  if (!isSelected()) {
    throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
  }
  int len;
  char buf[MAX_BUF_LEN + 1];
  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READONLY);
    // Comunicazione stato cartella nelle varie versioni:
    // - Si invia il numero totale di versioni
    // - Si inizia dalla versione minore (sempre la 1)
    // - Si invia il numero di versione (1\r\n)
    // - Seguito dal numero di file presenti nella versione
    // - Seguito dai dati dei file inclusi in quel backup, uno per riga
    // - Terminato l'invio per la prima versione si passa alla successiva (2)
    // - n volte quante sono le versioni diverse
    // Conto numero di versioni
    int n_vers = db.execAndGet("SELECT COUNT (DISTINCT Versione_BCK) FROM '" + table_name + "';");
    Logger::writeToLog("Invio informazioni relative al backup della cartella " + path + ", numero di versioni: " + to_string(n_vers));
    // Invio numero totale di versioni
    sprintf(buf, "%d\r\n", n_vers);
    send(s_c, buf, strlen(buf), 0);
    for (int i = 1; i <= n_vers; i++) {
      // Versione i
      // Invio numero versione
      sprintf(buf, "%d\r\n", i);
      send(s_c, buf, strlen(buf), 0);
      string _query("SELECT File_CL, Last_Modif, Hash, File_SRV FROM '" + table_name + "' WHERE Versione_BCK = ?;");
      SQLite::Statement query(db, _query);
      query.bind(1, i);
      int count = db.execAndGet("SELECT COUNT (*) FROM '" + table_name + "' WHERE Versione_BCK = " + to_string(i) + ";");
      // Invio del numero di file per la versione corrente
      sprintf(buf, "%d\r\n", count);
      send(s_c, buf, strlen(buf), 0);
      // Dato il numero di backup = i inviare per ogni file:
      for (int j = 0; j < count; j++) {
        query.executeStep();
        string nome = query.getColumn("File_CL");
        time_t timestamp = query.getColumn("Last_Modif");
        string hash = query.getColumn("Hash");
        string local_file = query.getColumn("File_SRV");
        File file(nome, (*this), hash, local_file);
        string full_path_file = file.getFullPath();
        Logger::writeToLog("Invio informazioni relative al file: " + full_path_file, DEBUG, CONSOLE_ONLY);
        // Invio: [Nome file]\r\n[Ultima modifica (16 char)][Hash (32 char)]\r\n
        int cur_file_len = full_path_file.length() + 2/*\r\n*/+ 16 /*Ultima modifica*/+ 32/*Hash*/+ 2/*\r\n*/+ 1/*\0*/;
        char cur_file[cur_file_len] = "";
        sprintf(cur_file, "%s\r\n%016lX%s\r\n", full_path_file.c_str(), timestamp, hash.c_str());
        /*string to_send(nome + "\r\n" + to_string(timestamp) + hash + "\r\n");*/
        send(s_c, cur_file, cur_file_len-1, 0);
        len = recv(s_c, buf, COMM_LEN, 0);
        if ((len == 0) || (len == -1)) {
          // Ricevuta stringa vuota: connessione persa
          throw runtime_error("connessione persa");
        }
        buf[len] = '\0';
        if (!strcmp(buf, "DATA_REC")) {
          Logger::writeToLog("Informazioni file ricevute dal client", DEBUG, CONSOLE_ONLY);
        } else {
          throw runtime_error("ricevuto comando sconosciuto");
        }
      }
      Logger::writeToLog("Informazioni relative alla versione " + to_string(i) + " inviate correttamente", DEBUG, CONSOLE_ONLY);
    }
    if (n_vers == 0){
      Logger::writeToLog("Nessuna versione trovata per la cartella " + path);
    } else {
      Logger::writeToLog("Informazioni relative alla cartella " + path + " inviate correttamente");
    }
    return;
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    return;
  }
}

// Rimuovi i dati relativi ai file presenti nella cartella
void Folder::removeFiles() {
  files.clear();
}

// Inserimento di un nuovo file nel backup corrente
bool Folder::newFileBackup(int s_c) {

  if (!isSelected()) {
    throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
  }
  int vrs;
  // Un nuovo file deve essere inserito nel backup corrente:
  // - Si trova la versione di backup corrente
  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
    string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
    SQLite::Statement query(db, _query);
    query.executeStep();
    vrs = query.getColumn(0);
    // Risultato =/= 0 -> corretto, si aggiunge all'ultimo backup
    if (vrs != 0) {
      vrs += 1;
      sendCommand(s_c, "BACKUPOK");
    } else {
      // Non esiste un backup completo corrente -> Errore
      Logger::writeToLog("Si sta tentando di aggiungere un file senza avere un backup completo", ERROR);
      sendCommand(s_c, "MISS_BCK");
      return false;
    }
    query.reset();
    // - Si apre un backup e una transazione sul DB
    Backup bck(vrs, (*this));
    SQLite::Transaction trs(db);
    // - Si copiano le voci relative all'ultima versione in una nuova versione con gli stessi file
    SQLCopyRows(db, (vrs - 1));
    // - Si riceve il file [receive_file]
    for (int tent = 1; tent < 6;) {
      if (receiveFile(s_c, vrs, db, string())) {
        // - Se il file e le informazioni sul DB sono state processate correttamente
        // - Commit DB e backup completato
        sendCommand(s_c, "DATA_OK_");
        trs.commit();
        bck.completed();
        Logger::writeToLog("Ricezione file completata");
        return true;
      } else {
        if (errno != EEXIST) {
          Logger::writeToLog("Errore nella ricezione: tentativo " + to_string(tent), ERROR);
          sendCommand(s_c, "SNDAGAIN");
          tent++;
          continue;
        } else {
          Logger::writeToLog("File già esistente", ERROR);
          sendCommand(s_c, "DATA_OK_");
          trs.commit();
          bck.completed();
          return true;
        }
      }
    }
    return false;
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    return false;
  }
}

// Ricezione di un nuovo file nella cartella
bool Folder::receiveFile(int s_c, int vrs, SQLite::Database& db, string folder_name) {

  int len;
  string filename;
  char buffer[MAX_BUF_LEN + 1] = "";
  char *file_info;
  char _size_c[12 + 1] = "", hash[32 + 1] = "", _timestamp_c[16 + 1] = "";
  size_t size;
  time_t timestamp;

  // Ricezione: [Nome completo file]\r\n[Dimensione file (12 char)][Hash del file (32 char)][Timestamp (8 char)]\r\n
  len = recv(s_c, buffer, MAX_BUF_LEN, 0);
  if (len == 0 || len == -1) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  buffer[len] = '\0';
  /* Estrazione nome, dimensione, hash, timestamp */
  filename = strtok(buffer, "\r\n");
  file_info = strtok(NULL, "\r\n");
  // Lettura dimensione
  strncpy(_size_c, file_info, 12);
  _size_c[12] = '\0';
  size = strtol(_size_c, NULL, 16);
  // Lettura Hash ricevuto
  strncpy(hash, file_info + 12, 32);
  hash[32] = '\0';
  // Lettura timestamp ricevuto
  strncpy(_timestamp_c, file_info + 44, 16);
  _timestamp_c[16] = '\0';
  timestamp = strtoul(_timestamp_c, NULL, 16);

  File file(filename, (*this), size, timestamp, hash);
  ///* DEBUG */cout << "Nome file: " << file.getName() << endl;
  ///* DEBUG */cout << "nella cartella " << file.getBasePath().getPath() << endl;
  ///* DEBUG */cout << "di dimensione " << file.getSize() << " Byte" << endl;
  ///* DEBUG */cout << "Percorso completo file: " << file.getFullPath() << endl;
  ///* DEBUG */timestamp = file.getTimestamp();
  ///* DEBUG */struct tm *time_info = gmtime(&timestamp);
  ///* DEBUG */time_info->tm_hour+=2;
  ///* DEBUG */strftime(buffer, MAX_BUF_LEN, "%A %e %B %Y, %H:%M:%S", time_info);
  ///* DEBUG */cout << "ultima modifica: " << buffer << endl;
  try {
    // - Elimina informazioni relative al file dalla versione corrente del backup, se presenti
    // SQL: DELETE FROM '[nome_table]' WHERE Versione_BCK=[vrs] AND Hash=file.getHash()
    string _query("DELETE FROM '" + table_name + "' WHERE Versione_BCK=" + to_string(vrs) + " AND File_CL='" + file.getName() + "';");
    SQLite::Statement query(db, _query);
    Logger::writeToLog("File: " + file.getFullPath() + ", Dimensione: " + to_string(file.getSize()) + " Byte, MD5: " + string(file.getHash()) + ", Timestamp: " + to_string(file.getTimestamp()),
        DEBUG, LOG_ONLY);
    if (query.exec() == 0) {
      // Non è un aggiornamento, ma un inserimento
      Logger::writeToLog("Il file non è un aggiornamento, ma un file nuovo", DEBUG, LOG_ONLY);
    } else {
      Logger::writeToLog("Il file è un aggiornamento", DEBUG, LOG_ONLY);
    }
    // Invio conferma ricezione dati
    sendCommand(s_c, "INFO_OK_");
    // Ricezione contenuto del file
    if (file.receiveFileData(s_c, folder_name)) {
      /* Inserimento file nel database */;
      string _query("INSERT INTO '" + table_name + "' (File_CL, Last_Modif, File_SRV, Hash, Versione_BCK) VALUES (?, ?, ?, ?, ?);");
      SQLite::Statement query(db, _query);
      query.bind(1, file.getName());
      query.bind(2, (long long int) file.getTimestamp());
      query.bind(3, file.getSavePath());
      query.bind(4, file.getHash());
      query.bind(5, vrs);
      if (query.exec() == 1) {
        Logger::writeToLog("Entry relativa al file inserita con successo");
        file.completed();
        insertFile(file);
        return true;
      } else {
        return false;
      }
    } else {
      return false;
    }
  } catch (ios_base::failure &e) {
    Logger::writeToLog("Errore nella scrittura del file: " + string(e.what()), ERROR);
    return false;
  }
}

// Copia delle voci relative all'ultima versione in una nuova versione con gli stessi file
void Folder::SQLCopyRows(SQLite::Database& db, int vrs) {
  // CREATE TABLE 'temporary_table' AS SELECT * FROM '[nome_table]' WHERE Versione_BCK= ?;
  string table("temporary_table" + user.getUser());
  string _query = "CREATE TABLE '" + table + "' AS SELECT * FROM '" + table_name + "' WHERE Versione_BCK= ?;";
  SQLite::Statement query_1(db, _query);
  query_1.bind(1, vrs);
  query_1.exec();
  // UPDATE 'temporary_table' SET Versione_BCK=(Versione_BCK+1);
  _query = "UPDATE '" + table + "' SET Versione_BCK=(Versione_BCK+1);";
  SQLite::Statement query_2(db, _query);
  query_2.exec();
  // UPDATE 'temporary_table' SET File_ID=NULL;
  _query = "UPDATE '" + table + "' SET File_ID=NULL;";
  SQLite::Statement query_3(db, _query);
  query_3.exec();
  // INSERT INTO '[nome_table]' SELECT * FROM 'temporary_table';
  _query = "INSERT INTO '" + table_name + "' SELECT * FROM '" + table + "';";
  SQLite::Statement query_4(db, _query);
  query_4.exec();
  // DROP TABLE 'temporary_table';
  _query = "DROP TABLE '" + table + "';";
  SQLite::Statement query_5(db, _query);
  query_5.exec();
}

// Eliminazione di un file dal backup corrente
bool Folder::deleteFileBackup(int s_c) {
  // Un file deve essere eliminato dal backup corrente:
  // - Si trova la versione di backup corrente
  if (!isSelected()) {
    throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
  }
  int vrs;
  char buffer[MAX_BUF_LEN];
  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
    string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
    SQLite::Statement query(db, _query);
    if (query.executeStep()) {
      // Un risultato = corretto, si aggiunge all'ultimo backup
      vrs = query.getColumn(0).getInt() + 1;
    } else {
      // Non esiste un backup completo corrente -> Errore
      Logger::writeToLog("Non esiste un backup completo corrente", ERROR);
      sendCommand(s_c, "MISS_BCK");
      return false;
    }
    query.reset();
    Logger::writeToLog("Backup trovato, versione corrente: " + to_string(vrs - 1));
    // - Si apre una transazione sul DB
    SQLite::Transaction trs(db);
    // - Si copiano le voci relative all'ultima versione in una nuova versione con gli stessi file
    SQLCopyRows(db, (vrs - 1));
    // - Ricezione nome del file da eliminare
    int len;
    len = recv(s_c, buffer, MAX_BUF_LEN, 0);
    if ((len == 0) || (len == -1)) {
      // Ricevuta stringa vuota: connessione persa
      throw runtime_error("connessione persa");
    }
    buffer[len] = '\0';
    string filename = string(buffer).substr(path.length()+1, string::npos);
    Logger::writeToLog("File da eliminare: " + filename);
    sendCommand(s_c, "INFO_OK_");
    _query.assign("DELETE FROM '" + table_name + "' WHERE File_CL = ? AND Versione_BCK = ?;");
    SQLite::Statement query_1(db, _query);
    query_1.bind(1, filename);
    query_1.bind(2, vrs);
    // - Nella versione nuova si elimina il file richiesto
    if (query_1.exec() == 1) {
      // Eliminazione corretta di UNA SOLA riga
      trs.commit();
      Logger::writeToLog("File rimosso correttamente dal backup corrente");
      sendCommand(s_c, "DELETED_");
      return true;
    } else {
      // Eliminazione di nessuna riga della tabella
      Logger::writeToLog("File non esistente", ERROR);
      sendCommand(s_c, "NOT_DEL_");
      return false;
    }
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    return false;
  }
  return false;
}

// Ricezione di un nuovo backup completo
bool Folder::fullBackup(int s_c) {

  if (!isSelected()) {
    throw runtime_error("si sta tentando di operare su una cartella senza averla selezionata");
  }
  int vrs, len;
  char comm[COMM_LEN];

  // Creazione nuova versione backup nel DB
  // Lettura versione attuale
  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
    string _query("SELECT MAX(Versione_BCK) FROM '" + table_name + "';");
    SQLite::Statement query(db, _query);
    // Esecuzione query -> una sola riga di risultato (SELECT MAX)

    if (query.executeStep())
      // Se c'è un risultato = esiste già un backup della cartella
      vrs = query.getColumn(0).getInt() + 1;
    else
      // Altrimenti: prima versione del backup
      vrs = 1;
    // Ricevere tutti  i file che il client mi manda, inserendoli in ordine di arrivo nel DB, con la versione corretta
    query.reset();

    Backup backup(vrs, (*this));
    SQLite::Transaction trs(db);
    string folder_name("./" + ServerSettings::getFilesFolder() + "/" + user.getUser() + "_" + path + "_FBck_" + to_string(time(NULL)));

    // Attesa comando: 	- se REC_FILE -> nuovo file in arrivo
    //					        - se SYNC_END -> fine backup
    for (;;) {
      len = recv(s_c, comm, COMM_LEN, 0);
      if ((len == 0) || (len == -1)) {
        // Ricevuta stringa vuota: connessione persa
        throw runtime_error("connessione persa");
      }
      comm[len] = '\0';
      if (!strcmp(comm, "REC_FILE")) {
        // Ricevi file
        if (receiveFile(s_c, vrs, db, folder_name)) {
          // File ricevuto correttamente
          sendCommand(s_c, "DATA_OK_");
          continue;
        } else {
          // Errore nella ricezione del file, richiesta nuova spedizione
          sendCommand(s_c, "SNDAGAIN");
          continue;
        }
      } else if (!strcmp(comm, "SYNC_END")) {
        // Fine positiva backup -> commit dell'intera transazione
        trs.commit();
        backup.completed();
        break;
      } else {
        // Comando sconosciuto
        sendCommand(s_c, "UNKNOWN_");
        break;
      }
    }
    if (backup.isComplete() && trs.isCommitted()) {
      // Se la transazione ha avuto commit e il backup è completo
      Logger::writeToLog("Backup ricevuto con successo");
      return true;
    } else {
      // Se la transazione non ha avuto commit
      // Rollback e cancellazione file
      Logger::writeToLog("Errore nella ricezione del backup, annullamento operazione");
      return false;
    }
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    return false;
  }
}

// Invio di un backup completo
void Folder::sendBackup(int s_c) {

  // Invio backup completo:
  // - Si attende che l'utente selezioni la versione desiderata
  char buffer[MAX_BUF_LEN + 1];
  // Versione
  int len = recv(s_c, buffer, MAX_BUF_LEN, 0);
  if ((len == 0) || (len == -1)) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  buffer[len] = '\0'; // -> es. buffer: "1\0"
  int vrs = strtol(buffer, NULL, 10); // -> es. vrs = 1
  try {
    SQLite::Database db(ServerSettings::getDBFile());
    int count = db.execAndGet("SELECT COUNT (*) FROM '" + table_name + "' WHERE Versione_BCK = " + to_string(vrs));
    SQLite::Statement query(db, "SELECT * FROM '" + table_name + "' WHERE Versione_BCK = ?");
    query.bind(2, vrs);
    // - Si spedisce la versione selezionata, un file per volta
    for (int i = 0; i < count; i++) {
      query.executeStep();
      // Lettura dati file
      string filename = query.getColumn("File_CL");		// Nome del file nel client
      string hash = query.getColumn("Hash");				// Hash del file
      string local_file = query.getColumn("File_SRV");	// Posizione del file sul file system
      // Oggetto 'file', senza timestamp (non serve in questo caso) e dimensione (non serve in questo caso)
      File file(filename, (*this), hash, local_file);
      file.sendFileData(s_c);
    }
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
  }
}

// Invio di un singolo file da un backup
void Folder::sendSingleFile(int s_c) {

  // Invio file singolo da backup:
  // - Si attende il nome del file richiesto dall'utente e il numero di versione
  char buffer[MAX_BUF_LEN];
  int len = recv(s_c, buffer, MAX_BUF_LEN, 0);
  if ((len == 0) || (len == -1)) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  buffer[len] = '\0'; // -> es. buffer: "file.txt\r\n1\0"
  string file_req = strtok(buffer, "\r\n"); // -> es. file_req = "file.txt"
  int vrs = strtol((strtok(NULL, "\r\n")), NULL, 10); // -> es. vrs = 1
  string filename = string(buffer).substr(path.length()+1, string::npos);
  Logger::writeToLog("Richiesto file " + filename + ", dalla versione " + to_string(vrs));
  // - Si spedisce il file richiesto
  try {
    SQLite::Database db(ServerSettings::getDBFile());
    SQLite::Statement query(db, "SELECT * FROM '" + table_name + "' WHERE File_CL = ? AND Versione_BCK = ?");
    query.bind(1, file_req);
    query.bind(2, vrs);
    query.executeStep();
    // Lettura dati file
    string filename = query.getColumn("File_CL");		// Nome del file nel client
    string hash = query.getColumn("Hash");				// Hash del file
    string local_file = query.getColumn("File_SRV");
    // Oggetto 'file', senza timestamp (non serve in questo caso) e dimensione (non serve in questo caso)
    File file(filename, (*this), hash, local_file);
    file.sendFileData(s_c);
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
  }
}
