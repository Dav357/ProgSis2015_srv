/*
 * Backup.h
 *
 *  Created on: 10 ago 2015
 *      Author: davide
 */

#ifndef BACKUP_H_
#define BACKUP_H_

bool full_backup(int, Folder&);

class Backup{
private:
	int vers;
	bool complete;
	Folder &folder;
public:
	Backup(int, Folder&);
	virtual ~Backup();
	void completed();
};

#endif /* BACKUP_H_ */
