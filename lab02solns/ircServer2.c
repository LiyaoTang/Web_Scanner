/*
 * select-based IRC server for comp3310
 * Peter Strazdins, RSCS ANU, 04/14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //bzero()
#include <sys/socket.h>
#include <unistd.h> //read(), write()
#include <netdb.h> //getaddrinfo()
#include <error.h> //error()
#include <sys/select.h>

#define MY_PORT "3310"
#define BUFLEN 128
#define MAX_CLIENTS 4

void perrExit(char *msg) {
    perror(msg);
    exit(1);
}

int maxArray(int a[], int n) {
  int i, mx = a[0];
  for (i=1; i < n; i++)
    if (a[i] > mx)
      mx = a[i];
  return mx;
}

int main(int argc, char ** argv) {
  struct addrinfo hints, *res;
  char buf[BUFLEN];

  int err, nbytes;
  int sock;
  int sockSet[MAX_CLIENTS+1]; //+1 for the listening socket
  int nSocks = 1; 
  fd_set rdSet; // reflects sockSet
  int nConnect=0, connectId[MAX_CLIENTS+1]; // keep ids of connections
 
  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_INET;/* or AF_INET6 ... */
  hints.ai_socktype = SOCK_STREAM;/* for tcp */
  hints.ai_flags = AI_PASSIVE;     //create socket wildcard address;
  if ((err = getaddrinfo(NULL, MY_PORT, &hints, &res))) 
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
	
  sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (bind(sock, (struct sockaddr *) res->ai_addr, res->ai_addrlen) == -1) 
    perrExit("bind");
  if (listen(sock, MAX_CLIENTS)) 
    perrExit("listen");
  sockSet[0] = sock;

  while (1) {
    int i;
    FD_ZERO(&rdSet);
    for (i = 0; i < nSocks; i++)
      FD_SET(sockSet[i], &rdSet);
    if (select(maxArray(sockSet, nSocks)+1, &rdSet, NULL, NULL, NULL) == -1)
      perrExit("select");

    for (i = 0; i < nSocks; i++) {
      if (FD_ISSET(sockSet[i], &rdSet)) {
	// printf("select on socket[%d.]=%d\n", i, sockSet[i]);
	if (i == 0) { 
	  int sock1 = accept(sock, NULL, NULL);
	  if (sock1 == -1) 
	    perrExit("accept");
	  if (nSocks > MAX_CLIENTS) {
	    sprintf(buf, "IRC error: too many clients!\n");
	    write(sock1, buf, strlen(buf));
	    close(sock1);
	  } else {
	    nConnect++;
	    connectId[nSocks] = nConnect;
	    sockSet[nSocks] = sock1;
	    nSocks++;
	  }
	} else { // echo read data to all clients
	  int j;
	  nbytes = read(sockSet[i], buf, sizeof(buf));
	  // printf("read %d bytes from socket[%d]=%d\n", nbytes, i, sockSet[i]);
	  if (nbytes != 0) {
	    char iBuf[8];
	    sprintf(iBuf, "%d> ", connectId[i]);
	    for (j = 1; j < nSocks; j++) {
	      write(sockSet[j], iBuf, strlen(iBuf));
	      write(sockSet[j], buf, nbytes);	   
	    }
	  } else { // delete sockSet[i] from list
	    close(sockSet[i]);
	    for (j = i; j < nSocks-1; j++)
	      sockSet[j] = sockSet[j+1];
	    nSocks--;	    
	  }
	}
      }
    } //for(i...)
  } //while(1)

  return (1);
} //main()
