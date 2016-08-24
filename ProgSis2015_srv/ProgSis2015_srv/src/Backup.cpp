/*
 * Backup.cpp
 *
 *  Created on: 10 ago 2015
 *      Author: Davide Locatelli
 */

#include "GenIncludes.hpp"
#include <set>
#include "Backup.h"
using namespace std;

void delEmptyFold();

// Costruttore
Backup::Backup(int v, Folder& f) :
        vers(v), complete(false), folder(f) {
}

// Distruttore
Backup::~Backup() {
  if (!complete) {
    // Se il backup non è completo:
    // Cancella tutti i file della cartella
    for (File f : folder.getFiles()) {
      remove(f.getSavePath().c_str());
    }
    folder.clearFolder();
  } else {
    // Altrimenti ripulisci solo il vettore dei file
    folder.removeFiles();
  }
}

// Conferma del completamento del backup
void Backup::completed() {
  complete = true;
}

// Funzione di pulizia del database alla disconnessione di un utente
void dbMaintenance(string table_name) {

  try {
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READWRITE);
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
        Logger::writeToLog("Aggiunto file " + cur_file + " all'elenco dei file da non eliminare", DEBUG, LOG_ONLY);
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
          Logger::writeToLog("Aggiunto file " + cur_file + " all'elenco dei file da eliminare", DEBUG, LOG_ONLY);
          cancellare.insert(cur_file);
        }
      }
    }
    // Si cancellano le righe relative alle vecchie versioni
    if (db.exec("DELETE FROM '" + table_name + "' WHERE Versione_BCK < " + to_string(min_vers) + " ;") == 0) {
      Logger::writeToLog("Database non aggiornato", DEBUG, LOG_ONLY);
      delEmptyFold();
      return;
    }
    Logger::writeToLog("Righe rimosse correttamente", DEBUG, LOG_ONLY);
    // Si cancellano i file indicati nel set 'cancellare'
    for (string f : cancellare) {
      remove(f.c_str());
      Logger::writeToLog("File " + f + " eliminato", DEBUG, LOG_ONLY);
    }
    Logger::writeToLog("Tutti i file obsoleti sono stati eliminati");

    // Si aggiornano i numeri di versione da min_vers a 1 a salire
    if (db.exec("UPDATE '" + table_name + "' SET Versione_BCK = (Versione_BCK - " + to_string(min_vers - 1) + ");") == 0) {
      Logger::writeToLog("Errore nell'aggiornamento della versione", ERROR, LOG_ONLY);
    } else {
      Logger::writeToLog("Numero di versione aggiornato correttamente", DEBUG, LOG_ONLY);
    }

    delEmptyFold();
    Logger::writeToLog("Pulizia del database terminata");

  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
  }
}

void delEmptyFold() {
  // Si eliminano le cartelle vuote rimaste, eventuali errori sono soppressi (es. la cartella con i file ricevuti è vuota)
  system(string("find ./" + ServerSettings::getFilesFolder() + "/* -empty -type d -delete 2> /dev/null").c_str());
  Logger::writeToLog("Eliminate cartelle vuote", DEBUG, LOG_ONLY);
}
