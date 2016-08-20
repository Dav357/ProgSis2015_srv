#include "GenIncludes.hpp"
#include "Account.h"

using namespace std;

enum loginReturnValues {
  TRUE, FALSE, EXISTING
};

/* Costruttori */
// Costruttore parziale, per inizializzazione
Account::Account() {
  usr = "";
  shapass = "";
  complete = false;
}
// Costruttore completo
Account::Account(string username, string password) {
  usr = username;
  shapass = password;
  complete = true;
}
/* Fine costruttori */

// Assegnazione nome utente all'account corrente (da stringa)
void Account::assignUsername(std::string username) {
  usr = username;
  if (!shapass.empty()) {
    complete = true;
  }
}

// Assegnazione nome utente all'account corrente (da puntatore a char)
void Account::assignUsername(char* username) {
  usr.assign(username);
  if (!shapass.empty()) {
    complete = true;
  }
}

// Assegnazione password all'account corrente (da stringa)
void Account::assignPassword(std::string password) {
  shapass = password;
  if (!usr.empty()) {
    complete = true;
  }
}

// Assegnazione password all'account corrente (da puntatore a char)
void Account::assignPassword(char* password) {
  shapass.assign(password);
  if (!usr.empty()) {
    complete = true;
  }
}

// Rimozione dei dati relativi all'account
void Account::clear() {
  if (!usr.empty()) removeFromUsertable(usr);
  usr.clear();
  shapass.clear();
  complete = false;

}

// Gestione account: login/creazione
char Account::accountManag(int flags) {
  // flags vale:
  // - 0x01 per login (equivale a DB in sola lettura)
  // - 0x02 per creazione account (DB in lettura/scrittura)
  string _query;
  SQLite::Database db(ServerSettings::getDBFile(), flags);
  if (flags == LOGIN) {
    // Login
    _query.assign("SELECT * FROM 'users' WHERE username = ? AND sha2_pass = ?;");
  } else {
    // Creazione account
    _query.assign("INSERT INTO 'users' ('username', 'sha2_pass') VALUES (?, ?);");
  }
  SQLite::Statement query(db, _query);
  query.bind(1, usr);
  query.bind(2, shapass);
  if (flags == LOGIN) {
    if (query.executeStep()) {
      Logger::writeToLog("Account " + usr + " trovato");
      if (!addToUsertable(usr))
        return EXISTING;
      else
        return TRUE;
    } else {
      Logger::writeToLog("Combinazione username/password per " + usr + " non trovata");
      clear();
      return FALSE;
    }
  } else {
    if (query.exec() == 1) {
      Logger::writeToLog("Account " + usr + " creato con successo, utente connesso");
      if (!addToUsertable(usr))
        return EXISTING;
      else
        return TRUE;
    } else {
      Logger::writeToLog("Errore nella creazione dell'account", ERROR);
      clear();
      return FALSE;
    }
  }
}

// Selezione di una cartella relativa all'account
Folder Account::selectFolder(int s_c) {

  int len;
  char buffer[MAX_BUF_LEN + 1] = "";
  string path;

  len = recv(s_c, buffer, MAX_BUF_LEN, 0);
  if ((len == 0) || (len == -1)) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  buffer[len] = '\0';
  path.assign(buffer);
  // Costruzione nome tabella relativa alla cartella considerata
  Folder f(path, (*this));
  try {
    // Apertura DB (sola lettura)
    SQLite::Database db(ServerSettings::getDBFile(), SQLITE_OPEN_READONLY);

    if (db.tableExists(f.getTableName())) {
      Logger::writeToLog("Trovata tabella relativa alla cartella");
      sendCommand(s_c, "FOLDEROK");
      return f;
    } else {
      Logger::writeToLog("Tabella relativa alla cartella " + path + " non trovata, creazione tabella");
      if (f.createTableFolder()) {
        sendCommand(s_c, "FOLDEROK");
        return f;
      } else {
        sendCommand(s_c, "FOLD_ERR");
        return (Folder());
      }
    }
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    return (Folder());
  }
  return (Folder());
}

// Funzione di login
Account login(int s_c, char *usertable, int rec) {

  int len, flags;
  char *buf;
  string usr, shapass;
  Account ac;
  char buffer[MAX_BUF_LEN + 1] = "", comm[COMM_LEN + 1] = "";
  len = recv(s_c, comm, COMM_LEN, 0);
  if ((len == 0) || (len == -1)) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  comm[len] = '\0';
  try {
    // Richiesta di login o richiesta di creazione account
    if (!strcmp(comm, "LOGIN___") || (!strcmp(comm, "CREATEAC"))) {
      Logger::writeToLog("Autenticazione in corso, in attesa dei dati di accesso");
      // Ricezione "[username]\r\n[sha-256_password]\r\n"
      len = recv(s_c, buffer, MAX_BUF_LEN, 0);
      if ((len == 0) || (len == -1)) {
        // Ricevuta stringa vuota: connessione persa
        throw runtime_error("connessione persa");
      }
      buffer[len] = '\0';
      // Inserimento in stringhe di username...
      buf = strtok(buffer, "\r\n");
      ac.assignUsername(buf);
      // ... e dell'SHA-256 della password
      buf = strtok(NULL, "\r\n");
      ac.assignPassword(buf);
      // Controllo lunghezza massima username, in caso negativo...
      if (ac.getUser().length() > ServerSettings::getMaxUsernameLen()) {
        // ...viene comunicata la lunghezza massima per il nome utente
        sprintf(comm, "MAXC_%03lu", ServerSettings::getMaxUsernameLen());
        sendCommand(s_c, comm);
        return (Account());
      }
      if (!strcmp(comm, "LOGIN___")) {
        if (rec == 0)
          flags = LOGIN;
        else {
          sendCommand(s_c, "LOGINERR");
          return (Account());
        }
      } else {
        flags = CREATEACC;
      }
      switch (ac.accountManag(flags)) {
        case TRUE:
          sendCommand(s_c, "LOGGEDOK");
          Logger::writeToLog("Utente " + ac.getUser() + " connesso");
          return ac;
        case FALSE:
          sendCommand(s_c, "LOGINERR");
          if (rec == 0) {
            return login(s_c, usertable, 1);
          }
          return (Account());
        case EXISTING:
          sendCommand(s_c, "ALRTHERE");
          return (Account());
      }
    } else {
      // Comando non riconosciuto
      sendCommand(s_c, "UNKNOWN_");
      return ac;
    }
  } catch (SQLite::Exception& e) {
    Logger::writeToLog("Errore DB: " + string(e.what()), ERROR);
    sendCommand(s_c, "DB_ERROR");
    ac.clear();
    return ac;
  }
  return (Account());
}

void test_func() {
  string a;
  char buf[100] = "";
  sprintf(buf, "MAXC_%03lu", ServerSettings::getMaxUsernameLen());
  a.assign(buf);
  cout << a << endl << a.length() << endl;
  cout << ServerSettings::getMMapSize() << endl;
}
