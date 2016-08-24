/*
 * File.h
 *
 *  Created on: 07 ago 2015
 *      Author: Davide Locatelli
 */

#ifndef FILE_H_
#define FILE_H_

#include "Folder.h"

// Classe relativa ad un file
class File {

private:
  // Riferimento all'oggetto che contiene informazioni riguardo alla cartella in cui si trova il file (REMOTO)
  Folder& base_path;
  // Nome del file
  std::string filename;
  // Dimensione file (in Byte)
  size_t size;
  // Timestamp ultima modifica file
  time_t timestamp;
  // Hash del file
  std::string hash;
  // File completo?
  bool complete;
  // Percorso locale
  std::string save_path;
public:
  File(std::string, Folder&, size_t, time_t, std::string);
  /* Costruttore per invio file */
  File(std::string, Folder&, time_t, std::string, std::string);
  /* -------------------------- */
  virtual ~File();
  std::string getFullPath();
  std::string getName() {
    return filename;
  }
  std::string getSavePath() {
    return save_path;
  }
  Folder& getBasePath() {
    return base_path;
  }
  size_t getSize() {
    return size;
  }
  time_t getTimestamp() {
    return timestamp;
  }
  std::string getHash() {
    return hash;
  }
  void completed();
  bool receiveFileData(int socket, std::string);
  void sendFileData(int socket);
  bool hashCheck(int fpointer);
};

#endif /* FILE_H_ */
