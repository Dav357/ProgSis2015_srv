/*
 * File.cpp
 *
 *  Created on: 07 ago 2015
 *      Author: Davide Locatelli
 */

#include "GenIncludes.hpp"
#include "File.h"
#include <sys/stat.h>
#include <openssl/md5.h>
using namespace std;

// Costruttore
File::File(string fullpath, Folder& folder, size_t size, time_t timestamp, string h) :
    base_path(folder) {

  string name(fullpath, folder.getPath().length() + 1, string::npos);

  filename = name;
  this->size = size;
  this->timestamp = timestamp;
  hash = h;
  complete = false;
}

// Costruttore per invio file
File::File(string filename, Folder& folder, time_t t, string h, string savepath) :
    base_path(folder) {

  this->filename = filename;
  size = 0;
  timestamp = t;
  hash = h;
  save_path = savepath;
  complete = true;
}

// Costruzione percorso completo del file
string File::getFullPath() {
  string temp(base_path.getPath() + "\\" + filename);
  return temp;
}

// Conferma che il file è completo
void File::completed() {
  complete = true;
}

// Distruttore
File::~File() {
  // Se la ricezione non è completa -> CANCELLAZIONE file.
  if (!complete) {
    string fold_base(save_path);
    remove(save_path.c_str());
    string::size_type i = fold_base.find(filename);
    fold_base.erase(i, string::npos);
    rmdir(fold_base.c_str());
  }
}

// Ricezione dei byte che compongono il file
bool File::receiveFileData(int s_c, string folder_name) {

  int fpoint;
  time_t t = time(NULL);
  char buffer[MAX_BUF_LEN + 1];

  // Costruzione nome nuovo file
  string path_to_file;
  if (folder_name.empty()) {
    // Se il nome è vuoto, ricezione di un solo file nuovo/aggiornato -> cartella nuova
    path_to_file.assign("./" + ServerSettings::getFilesFolder() + "/" + base_path.getUser() + "_" + base_path.getPath() + "_" + to_string(t));
  } else {
    // Se è presente un percorso -> backup completo, tutti i file in una sola cartella
    path_to_file.assign(folder_name);
  }
  // Creazione cartella permessi: -rwxrw-r-- (764)
  mkdir(path_to_file.c_str(), S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH);
  path_to_file.append("/");

  // Costruzione nome file
  string fullpath(path_to_file + filename);
  save_path = fullpath;

  //Apertura file: in sola scrittura, crea se non esistente; permessi: -rwxrw-r-- (764)
  if ((fpoint = open(fullpath.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
    Logger::writeToLog("Errore nella creazione del file", ERROR);
    return false;
  } else {
    Logger::writeToLog("File " + fullpath + " creato correttamente, ricezione file in corso");
    //Loop di lettura bytes inviati dal client
    int rbyte = 0;
    size_t received = 0;
    for (;;) {
      rbyte = recv(s_c, buffer, MAX_BUF_LEN, 0);
      if (rbyte == 0 || rbyte == -1) break;
      write(fpoint, buffer, rbyte);
      received += rbyte;
      if (received == size) {
        if (this->hashCheck(fpoint)) {
          close(fpoint);
          Logger::writeToLog("L'hash del file ricevuto corrisponde all'hash fornito dal client");
          return true;
        } else {
          Logger::writeToLog("L'hash del file ricevuto non corrisponde all'hash fornito dal client", ERROR);
          close(fpoint);
          throw ios_base::failure("hash non corrispondente, cancellazione");
        }
      }
    }
    close(fpoint);
    throw ios_base::failure("ricezione parziale, cancellazione");
  }
}

// Controllo integrità del file (MD5)
bool File::hashCheck(int fpoint) {
  // Calcolo MD5 file
  // Lettura MD5 dalle informazioni ricevute relative al file
  // Valore di ritorno: confronto dei 2 MD5
  string remote_md5 = hash;

  lseek(fpoint, 0, SEEK_SET);
  char buffer[MAX_BUF_LEN + 1];
  unsigned char hash[MD5_DIGEST_LENGTH];
  char hash_str[2 * MD5_DIGEST_LENGTH + 1];
  MD5_CTX c;
  MD5_Init(&c);
  int result = read(fpoint, buffer, MAX_BUF_LEN);
  while (result > 0) {
    MD5_Update(&c, buffer, result);
    result = read(fpoint, buffer, MAX_BUF_LEN);
  }
  MD5_Final(hash, &c);
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    sprintf(hash_str + (2 * i), "%02X", hash[i]);
  }
  string local_md5(hash_str);
  Logger::writeToLog("File " + filename + ": MD5 ricevuto: " + remote_md5 + ", MD5 calcolato: " + local_md5);
  return ((local_md5.compare(remote_md5) == 0) ? true : false);
}

// Invio dei byte che compongono il file
void File::sendFileData(int s_c) {

  char comm[COMM_LEN + 1] = "", buffer[MAX_BUF_LEN + 1] = "";
  int fpoint = open(save_path.c_str(), O_RDONLY);
  size = lseek(fpoint, 0, SEEK_END);
  sprintf(buffer, "%s\r\n%lX\r\n%016lX\r\n", hash.c_str(), size, timestamp);
  if (send(s_c, buffer, strlen(buffer), 0) == -1) throw runtime_error("connessione persa");
  int len = recv(s_c, comm, COMM_LEN, 0);
  if ((len == 0) || (len == -1)) {
    // Ricevuta stringa vuota: connessione persa
    throw runtime_error("connessione persa");
  }
  comm[len] = '\0';
  if (!strcmp(comm, "DATA_REC")) {
    Logger::writeToLog("Hash del file ricevuto dal client", DEBUG, CONSOLE_ONLY);
  } else {
    // Comando sconosciuto
    sendCommand(s_c, "UNKNOWN_");
    throw runtime_error("ricevuto comando sconosciuto: " + string(comm));
  }
  for (int i = 1; i < 6; i++) {
    lseek(fpoint, 0, SEEK_SET);
    while ((len = read(fpoint, buffer, MAX_BUF_LEN)) > 0) {
      if (send(s_c, buffer, len, 0) == -1) throw runtime_error("connessione persa");
    }
    len = recv(s_c, comm, COMM_LEN, 0);
    if ((len == 0) || (len == -1)) {
      // Ricevuta stringa vuota: connessione persa
      throw runtime_error("connessione persa");
    }
    comm[len] = '\0';
    if (!strcmp(comm, "DATA_OK_")) {
      // File spedito correttamente
      close(fpoint);
      Logger::writeToLog("File ricevuto correttamente dal client");
      return;
    } else if (!strcmp(comm, "SNDAGAIN")) {
      // Errore nella spedizione del file, ripetere
      Logger::writeToLog("Richiesta nuova spedizione del file, tentativo: " + (i + 1), DEBUG, CONSOLE_ONLY);
      continue;
    } else {
      // Comando sconosciuto
      sendCommand(s_c, "UNKNOWN_");
      throw runtime_error("ricevuto comando sconosciuto: " + string(comm));
    }
  }
}
