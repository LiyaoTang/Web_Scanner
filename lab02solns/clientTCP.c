/* simple TCP client for comp3310
 * Peter Strazdins, RSCS ANU, 04/14; updated 03/18
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h> //bzero()
#include <sys/socket.h>
#include <netdb.h> //getaddrinfo()
#include <unistd.h> //close()
#include <error.h> //error()
#include <arpa/inet.h> //inet_ntoa(),ntohs()

#define PORT "3310"
#define BUFLEN 64
#define PROG "client"

#define MIN(a, b) ((a) <= (b)? (a): (b))

void resourceError(int status, char *caller) {
  printf("%s: resource error status=%d\n", PROG, status);
  perror(caller);
  exit(2);
}

#if 0
#define SYSCALL_CHECK(rv, failCond, caller, args) \
  do { \
    if (((rv) = caller args) failCond) \
      resourceError(rv, #caller); \
  } while (0)
// you can use this to make the C code to make and check a syscall
// more succinct. For example:
  SYSCALL_CHECK(sock, < 0, socket, (server->ai_family, server->ai_socktype, server->ai_protocol));
// is equivalent to:
  sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (sock < 0)
    resourceError(sock, "socket");
*/
#endif

// IPPROTO_TCP = 6,    /* Transmission Control Protocol.  */

int main(int argc, char* argv[]) {
  char *port;
  int err, nbytes;
  int sock; //client socket
  struct addrinfo hints, *server;
  struct sockaddr_in serverAddr; //info of sock; see /usr/include/linux/in.h
  socklen_t addrlen;  
  char buf[BUFLEN];
  char *bufr = NULL; size_t zero = 0;
  
  port = (argc == 1)? PORT: argv[1];
  
  // set up server's addrinfo: on this node, port MY_PORT, with following hints
  bzero(&hints, sizeof(hints));     // use defaults unless overridden
  hints.ai_family = AF_INET;        // IPv4; for IPv6, use AF_INET6 
  hints.ai_socktype = SOCK_STREAM;  // for TCP
  hints.ai_protocol = 0;            // any protocol

  if ((err = getaddrinfo(NULL/*this node*/, port, &hints, &server)))
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
  printf("%s: created addrinfo of type, family, protocol = %d,%d,%d \n (expect AF_INET=%d,SOCK_STREAM=%d,ANY=%d)\n", 
          PROG, server->ai_family, server->ai_socktype, server->ai_protocol, AF_INET, SOCK_STREAM, 0);

  //create client's socket, which must have the same family, type & protocol
  sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (sock < 0)
    resourceError(sock, "socket");
  printf("%s: socket created=%d\n", PROG, sock);

  //attempt to connect to the server
  if ((err = connect(sock, server->ai_addr, server->ai_addrlen)) < 0)
    resourceError(err, "connect");
  addrlen = sizeof(serverAddr);
  getsockname(sock, (struct sockaddr *) &serverAddr, &addrlen);
  printf("%s: client connected to server at %s, using port %d\n", PROG,
	 inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

  while ((nbytes = getline(&bufr, &zero, stdin)) != EOF) {
    nbytes = send(sock, bufr, nbytes, 0);
    if (nbytes < 0) 
      resourceError(nbytes, "send");
    // printf("%s: sent message (%d bytes): %s\n", PROG, nbytes, buf);

    //receive reply from server
    nbytes = recv(sock, buf, sizeof(buf), 0);
    if (nbytes < 0) 
      resourceError(nbytes, "recv");
    assert(nbytes <= sizeof(buf)); //check against buffer overflow!
    buf[MIN(nbytes, sizeof(buf)-1)] = 0;
    // do not trust server to send a null-terminated string!  
    printf("%s", buf);

    free(bufr); bufr = NULL;
  }
  printf("nbytes=%d\n", nbytes); 
  
  //close socket etc and terminate
  close(sock);
  freeaddrinfo(server);
  printf("%s: closed socket and terminating\n", PROG);
  return 0;
} //main()


