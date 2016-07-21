/*
 * Backup.cpp
 *
 *  Created on: 10 ago 2015
 *      Author: davide
 */

#include "GenIncludes.hpp"
#include <set>
#include "Backup.h"
using namespace std;

void del_empty_fold();

// Costruttore
Backup::Backup(int v, Folder& f) :
        vers(v), complete(false), folder(f) {
}

// Distruttore
Backup::~Backup() {
  if (!complete) {
    // Se il backup non è completo:
    // Cancella tutti i file della cartella
    for (File f : folder.get_files()) {
      remove(f.getSavePath().c_str());
    }
    folder.clear_folder();
  } else {
    // Altrimenti ripulisci solo il vettore dei file
    folder.remove_files();
  }
}

// Conferma del completamento del backup
void Backup::completed() {
  complete = true;
}

// Funzione di pulizia del database alla disconnessione di un utente
void db_maintenance(string table_name) {

  try {
    SQLite::Database db("database.db3", SQLITE_OPEN_READWRITE);
    // Minima versione da non eliminare (i cui file non sono da eliminare
    int max_vers = db.execAndGet("SELECT COUNT(DISTINCT Versione_BCK) FROM '" + table_name + "';");
    int min_vers = max_vers - 4;
    // Set contenente i file da NON cancellare
    set<string> non_cancellare;
    // Si aggiungono ai file da non cancellare quelli nelle ultime 5 versioni (tra min_vers e max_vers)
    for (int vrs = min_vers; vrs <= max_vers; vrs++) {
      SQLite::Statement query(db, "SELECT File_SRV FROM '" + table_name + "' WHERE Versione_BCK = ?");
      query.bind(1, vrs);
      while (query.executeStep()) {
        string cur_file = query.getColumn("File_SRV");
        Logger::write_to_log("Aggiunto file " + cur_file + " all'elenco dei file da non eliminare", DEBUG, LOG_ONLY);
        non_cancellare.insert(cur_file);
      }
    }
    // Set contenente i file da CANCELLARE
    set<string> cancellare;
    // Si aggiungono ai file da cancellare quelli nelle versioni più vecchie di min_vers...
    for (int vrs = 1; vrs < min_vers; vrs++) {
      SQLite::Statement query(db, "SELECT File_SRV FROM '" + table_name + "' WHERE Versione_BCK = ? ;");
      query.bind(1, vrs);
      while (query.executeStep()) {
        // ...che non compaiono nella lista dei file da non eliminare
        string cur_file = query.getColumn("File_SRV");
        if ((non_cancellare.find(cur_file)) == non_cancellare.end()) {
          Logger::write_to_log("Aggiunto file " + cur_file + " all'elenco dei file da eliminare", DEBUG, LOG_ONLY);
          cancellare.insert(cur_file);
        }
      }
    }
    // Si cancellano le righe relative alle vecchie versioni
    if (db.exec("DELETE FROM '" + table_name + "' WHERE Versione_BCK < " + to_string(min_vers) + " ;") == 0) {
      Logger::write_to_log("Database non aggiornato", DEBUG, LOG_ONLY);
      del_empty_fold();
      return;
    }
    Logger::write_to_log("Righe rimosse correttamente", DEBUG, LOG_ONLY);
    // Si cancellano i file indicati nel set 'cancellare'
    for (string f : cancellare) {
      remove(f.c_str());
      Logger::write_to_log("File " + f + "eliminato", DEBUG, LOG_ONLY);
    }
    Logger::write_to_log("Tutti i file obsoleti sono stati eliminati");

    // Si aggiornano i numeri di versione da min_vers a 1 a salire
    if (db.exec("UPDATE '" + table_name + "' SET Versione_BCK = (Versione_BCK - " + to_string(min_vers - 1) + ");") == 0) {
      Logger::write_to_log("Errore nell'aggiornamento della versione", ERROR, LOG_ONLY);
    } else {
      Logger::write_to_log("Numero di versione aggiornato correttamente", DEBUG, LOG_ONLY);
    }

    del_empty_fold();
    Logger::write_to_log("Pulizia del database terminata");

  } catch (SQLite::Exception& e) {
    Logger::write_to_log("Errore DB: " + string(e.what()), ERROR);
  }
}

void del_empty_fold() {
  // Si eliminano le cartelle vuote rimaste, eventuali errori sono soppressi (es. la cartella ReceivedFiles è vuota)
  system("find ./ReceivedFiles/* -empty -type d -delete 2> /dev/null");
  Logger::write_to_log("Eliminate cartelle vuote", DEBUG, LOG_ONLY);
}
