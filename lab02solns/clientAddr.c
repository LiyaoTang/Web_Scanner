/* simple TCP client for comp3310
 * Peter Strazdins, RSCS ANU, 04/14; updated 03/18
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h> //bzero()
#include <sys/socket.h>
#include <netdb.h> //getaddrinfo(),getnameinfo()
#include <unistd.h> //close()
#include <error.h> //error()
#include <arpa/inet.h> //inet_ntoa(),ntohs()

#define PORT "3310"
#define BUFLEN 64
#define PROG "client"

//#define SERVER "localhost"

//#define SERVER "cecs-042182" /*Peter's workstation*/
//#define SERVER "150.203.210.97" /*Peter's workstation*/
#define SERVER "fe80::da9e:f3ff:fe05:7f1b%2" /*Peter's workstation*/

//#define SERVER "150.203.24.13" /*partch*/
//#define SERVER "fe80::baca:3aff:febb:849a%2" /*partch*/

//#define SERVER "192.168.111.11"
//#define SERVER "fe80::16b3:1fff:fe28:f37f%2" /*N111.11*/


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

int main(int argc, char* argv[]) {
  char *port;
  int err, nbytes;
  int sock; //client socket
  struct addrinfo hints, *server;
  struct sockaddr_in clientAddr; //info of sock; see /usr/include/linux/in.h
  socklen_t addrlen;  
  char serverinfo[128], hostinfo[128];
  char buf[BUFLEN];

  port = (argc == 1)? PORT: argv[1];
  
  //set up server's addrinfo: on this node, port MY_PORT, with following hints
#define A_FAMILY AF_UNSPEC
  bzero(&hints, sizeof(hints));     //use defaults unless overridden
  hints.ai_family = A_FAMILY;       //IPv4; for IPv6, use AF_INET6; AF_UNSPEC 
  hints.ai_socktype = SOCK_STREAM; //for TCP
  hints.ai_protocol = 0;           //any protocol
  if ((err = getaddrinfo(SERVER, port, &hints, &server)))
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));
  printf("%s: created addrinfo of family,type,protocol=%d,%d,%d "
	 "(hints %d,SOCK_STREAM=%d,ANY=%d)\n", PROG,
         server->ai_family, server->ai_socktype, server->ai_protocol,
	 A_FAMILY, SOCK_STREAM, 0);

  err = 0;
  do {
    //create client's socket, which must have the same family, type & protocol
    sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock < 0)
      continue;
    printf("%s: socket created=%d\n", PROG, sock);

    getnameinfo(server->ai_addr, server->ai_addrlen, hostinfo, 128,
		serverinfo, 128, 0);
    printf("try connecting to %s on port %s\n", hostinfo, serverinfo);
    //attempt to connect to the server
    if ((err = connect(sock, server->ai_addr, server->ai_addrlen)) < 0)
      close(sock);
    else
      break;    
    server = server->ai_next;
  } while (server != NULL);

  if (server == NULL)
    resourceError(err, "connect");
  
  addrlen = sizeof(clientAddr);
  getsockname(sock, (struct sockaddr *) &clientAddr, &addrlen);
  printf("%s: client at %s connected to server, using port %d\n", PROG,
	 inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

  //create and send a message to server
  strncpy(buf, "Hello Server", sizeof(buf));
  nbytes = send(sock, buf, sizeof(buf), 0);
  if (nbytes < 0) 
    resourceError(nbytes, "send");
  printf("%s: sent message (%d bytes): %s\n", PROG, nbytes, buf);

  //receive reply from server
  nbytes = recv(sock, buf, sizeof(buf), 0);
  if (nbytes < 0) 
    resourceError(nbytes, "recv");
  assert(nbytes <= sizeof(buf)); //check against buffer overflow!
  buf[MIN(nbytes, sizeof(buf)-1)] = 0;
  // do not trust server to send a null-terminated string either!  
  printf("%s: received message (%d bytes): %s\n", PROG, nbytes, buf);

  //close socket etc and terminate
  close(sock);
  freeaddrinfo(server);
  printf("%s: closed socket and terminating\n", PROG);
  return 0;
} //main()


