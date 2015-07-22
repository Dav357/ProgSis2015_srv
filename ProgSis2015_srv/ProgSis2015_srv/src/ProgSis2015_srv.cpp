//============================================================================
// Name        : ProgSis2015_srv.cpp
// Author      : Davide
// Version     :
// Copyright   :
// Description : Main program
//============================================================================

#include <iostream>
#include <cstdlib>
#include <sys/socket.h>


#define COMM_LEN 35
#define FILE_NAME_LEN 30

using namespace std;

void terminate_handler() {
	cerr<<"Chiusura forzata processo\n";
	exit (-1);
}

int main() {
	int port, result;
	char comm[COMM_LEN], nmfile[FILE_NAME_LEN];
	struct sockaddr_in saddr, claddr = { 0 };
	socklen_t claddr_len = sizeof(struct sockaddr_in);
	int s, s_c; //Socket




	return 0;
}
