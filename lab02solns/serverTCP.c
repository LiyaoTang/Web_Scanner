/* simple TCP server for comp3310
 * Peter Strazdins, RSCS ANU, 03/18
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
#define PROG "server"

#define MIN(a, b) ((a) <= (b)? (a): (b))

void resourceError(int status, char *caller) {
  printf("%s: resource error status=%d\n", PROG, status);
  perror(caller);
  exit(2);
}

// IPPROTO_TCP = 6,    /* Transmission Control Protocol.  */

int main(int argc, char* argv[]) {
  char *port;
  int err, nbytes;
  int sock, sock2; // server listen and accept sockets
  struct addrinfo hints, *server;
  struct sockaddr_in serverAddr; // info of sock; see /usr/include/linux/in.h
  struct sockaddr_in clientAddr; // info on accepted client socket
  socklen_t addrlen;
  char buf[BUFLEN];

  port = (argc == 1)? PORT: argv[1];

  // set up server's addrinfo: on this node, port PORT, with following hints
  bzero(&hints, sizeof(hints));     // use defaults unless overridden
  hints.ai_family = AF_INET;        // IPv4; for IPv6, use AF_INET6 
  hints.ai_socktype = SOCK_STREAM;  // for TCP
  hints.ai_protocol = 0;            // any protocol
  if ((err = getaddrinfo(NULL/*this node*/, port, &hints, &server)))
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
  printf("%s: created addrinfo of type,family,protocol=%d,%d,%d "
	 "(expect AF_INET=%d,SOCK_STREAM=%d,ANY=%d)\n", PROG,
         server->ai_family, server->ai_socktype, server->ai_protocol,
	 AF_INET, SOCK_STREAM, 0);

  // create client's socket, which must have the same family, type & protocol
  sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (sock < 0)
    resourceError(sock, "socket");
  printf("%s: socket created=%d\n", PROG, sock);

  // attempt to bind & listen on specified port
  if ((err = bind(sock, server->ai_addr, server->ai_addrlen)) != 0)
    resourceError(err, "bind");

  if ((err = listen(sock, 1)) != 0)
    resourceError(err, "listen");
  addrlen = sizeof(serverAddr);
  getsockname(sock, (struct sockaddr *) &serverAddr, &addrlen);
  printf("%s: successfully bound and listening on port %d\n", 
	 PROG, ntohs(serverAddr.sin_port));

  //now block, waiting for a connection
  //if not interested in client details, just use accept(sock, NULL, NULL)
  addrlen = sizeof(clientAddr);
  sock2 = accept(sock, (struct sockaddr *) &clientAddr, &addrlen);
  if (sock2 < 0)
    resourceError(sock2, "accept");
  printf("%s: received connection from %s, now on port %d\n", 
	 PROG, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

  while (1) {
    //receive message from client
    nbytes = recv(sock2, buf, sizeof(buf), 0);
    if (nbytes < 0)
      resourceError(nbytes, "recv");
    else if (nbytes == 0)
      break;
    assert(nbytes <= sizeof(buf)); //check against buffer overflow!
    buf[MIN(nbytes, sizeof(buf)-1)] = 0;
    // do not trust client to send a null-terminated string!  
    printf("%s: received message (%d bytes): %s\n", PROG, nbytes, buf);
  
    nbytes = send(sock2, buf, nbytes, 0);
    if (nbytes < 0) 
      resourceError(nbytes, "send");
    printf("%s: sent message (%d bytes): %s\n", PROG, nbytes, buf);
  }
    
   //close sockets etc and terminate
  close(sock);
  close(sock2);
  freeaddrinfo(server);
  printf("%s: closed socket and terminating\n", PROG);
  return 0;
} //main()


