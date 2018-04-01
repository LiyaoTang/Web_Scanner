
/*
 * simple IRC server for comp3310
 * Peter Strazdins, RSCS ANU, 04/14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //bzero()
#include <sys/socket.h>
#include <unistd.h> //read(), write()
#include <netdb.h> //getaddrinfo()
#include <error.h> //error()

#define MY_PORT "3310"
#define BUFLEN 64

void perrExit(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char ** argv) {
   int err, nbytes;
   int sock, sock1, sock2;
  struct addrinfo hints, *res;
  char buf[BUFLEN];
   
  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_INET;/* or AF_INET6 ... */
  hints.ai_socktype = SOCK_STREAM;/* for tcp */
  hints.ai_flags = AI_PASSIVE;     //create socket wildcard address;
  if ((err = getaddrinfo(NULL, MY_PORT, &hints, &res))) 
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
	
  sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (bind(sock, (struct sockaddr *) res->ai_addr, res->ai_addrlen) == -1) 
    perrExit("bind");
  if (listen(sock, 1)) 
    perrExit("listen");
  
  sock1 = accept(sock, NULL, NULL);
  if (sock1 == -1) 
    perrExit("accept (1)");
  sock2 = accept(sock, NULL, NULL);
  if (sock2 == -1) 
    perrExit("accept (2)");

  while ((nbytes = read(sock1, buf, sizeof(buf))) != 0) {
    write(sock1, buf, nbytes);
    write(sock2, buf, nbytes);
  }

  close(sock2);
  close(sock1);
  close(sock);
  return (1);
} //main()
