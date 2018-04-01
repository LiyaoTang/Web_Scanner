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

int main(int argc, char* argv[]) {
  char *port;
  int err, nbytes;
  int sock; //server socket
  struct addrinfo hints, *server;
  struct sockaddr_in serverAddr; //info of sock; see /usr/include/linux/in.h
  struct sockaddr_in clientAddr; //info on accepted client socket
  socklen_t addrlen;
  char buf[BUFLEN];

  port = (argc == 1)? PORT: argv[1];

  //set up server's addrinfo: on this node, port PORT, with following hints
  bzero(&hints, sizeof(hints));     //use defaults unless overridden
  hints.ai_family = AF_INET;       //IPv4; for IPv6, use AF_INET6 
  hints.ai_socktype = SOCK_DGRAM;  //for UDP
  hints.ai_protocol = 0;           //any protocol
  if ((err = getaddrinfo(NULL/*this node*/, port, &hints, &server)))
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
  printf("%s: created addrinfo of type,family,protocol=%d,%d,%d "
	 "(expect AF_INET=%d,SOCK_DGRAM=%d,ANY=%d)\n", PROG,
         server->ai_family, server->ai_socktype, server->ai_protocol,
	 AF_INET, SOCK_DGRAM, 0);

  //create client's socket, which must have the same family, type & protocol
  sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (sock < 0)
    resourceError(sock, "socket");
  printf("%s: socket created=%d\n", PROG, sock);

  //attempt to bind and listen on specified port
  if ((err = bind(sock, server->ai_addr, server->ai_addrlen)) != 0)
    resourceError(err, "bind");
  addrlen = sizeof(serverAddr);
  getsockname(sock, (struct sockaddr *) &serverAddr, &addrlen);
  printf("%s: successfully bound on port %d\n", 
	 PROG, ntohs(serverAddr.sin_port));

  while (1) {
    //receive message from client
    addrlen = sizeof(clientAddr);    
    nbytes = recvfrom(sock, buf, sizeof(buf), 0,
		      (struct sockaddr *) &clientAddr, &addrlen);
    if (nbytes < 0)
      resourceError(nbytes, "recvfrom");
    else if (nbytes == 0)
      break;
    assert(nbytes <= sizeof(buf)); //check against buffer overflow!
    buf[MIN(nbytes, sizeof(buf)-1)] = 0;
    // do not trust client to send a null-terminated string!  
    printf("%s: received message from %s on port %d (%d bytes): %s\n", PROG,
	   inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port),
	   nbytes, buf);
  
    nbytes = sendto(sock, buf, nbytes, 0,
		    (struct sockaddr *) &clientAddr, addrlen);
    if (nbytes < 0) 
      resourceError(nbytes, "sendto");
    printf("%s: sent message (%d bytes): %s\n", PROG, nbytes, buf);
  }
    
  //close sockets etc and terminate
  close(sock);
  freeaddrinfo(server);
  printf("%s: closed socket and terminating\n", PROG);
  return 0;
} //main()


