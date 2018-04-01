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

// /* Socket types. */
// #define SOCK_STREAM 1       /* stream (connection) socket */
// #define SOCK_DGRAM  2       /* datagram (conn.less) socket  */
// #define SOCK_RAW  3         /* raw socket     */
// #define SOCK_RDM  4         /* reliably-delivered message */
// #define SOCK_SEQPACKET  5   /* sequential packet socket */
// #define SOCK_PACKET 10      /* linux specific way of  */
//                             /* getting packets at the dev */
//                             /* level.  For writing rarp and */
//                             /* other similar things on the  */
//                             /* user level.      */

/* Structure to contain information about address of a service provider.  */
// struct addrinfo
// {
//   int ai_flags;              /* Input flags.  */
//   int ai_family;             /* Protocol family for socket.  */
//   int ai_socktype;           /* Socket type.  */
//   int ai_protocol;           /* Protocol for socket.  */
//   socklen_t ai_addrlen;      /* Length of socket address.  */
//   struct sockaddr *ai_addr;  /* Socket address for socket.  */
//   char *ai_canonname;        /* Canonical name for service location.  */
//   struct addrinfo *ai_next;  /* Pointer to next in list.  */
// };
// #endif

// /* Possible values for `ai_flags' field in `addrinfo' structure.  */
// #ifndef AI_PASSIVE
// # define AI_PASSIVE 0x0001     /* Socket address is intended for `bind'.  */
// #endif
// #ifndef AI_CANONNAME
// # define AI_CANONNAME 0x0002   /* Request for canonical name.  */
// #endif
// #ifndef AI_NUMERICSERV
// # define AI_NUMERICSERV 0x0400 /* Don't use name resolution.  */
// #endif

// struct sockaddr_in:
// /* Structure describing an Internet (IP) socket address. */
// #if  __UAPI_DEF_SOCKADDR_IN
// #define __SOCK_SIZE__ 16            /* sizeof(struct sockaddr)  */
// struct sockaddr_in {
//   __kernel_sa_family_t  sin_family; /* Address family   */
//   __be16    sin_port;               /*  Port number   */
//   struct in_addr  sin_addr;         /* Internet address   */

//   /* Pad to size of `struct sockaddr' */
//   unsigned char   __pad[__SOCK_SIZE__ - sizeof(short int) -
//       sizeof(unsigned short int) - sizeof(struct in_addr)];
// };
// #define sin_zero  __pad             /* for BSD UNIX comp. -FvK  */
// #endif

int main(int argc, char* argv[]) {
  char *port;
  int err, nbytes;
  int sock, sock2;                // server listen and accept sockets
  struct addrinfo hints, *server; 
  socklen_t addrlen;              // feild in struct addrinfo: Length of socket address 
  struct sockaddr_in serverAddr;  // info of sock; see /usr/include/linux/in.h
  struct sockaddr_in clientAddr;  // info on accepted client socket

  char buf[BUFLEN];

  port = (argc == 1)? PORT: argv[1];

  // set up server's addrinfo: on this node, port PORT, with following hints
  bzero(&hints, sizeof(hints));     // use defaults unless overridden
  hints.ai_family = AF_INET;        // IPv4; for IPv6, use AF_INET6 
  hints.ai_socktype = SOCK_STREAM;  // for TCP
  hints.ai_protocol = 0;            // any protocol

  // getaddrinfo(): return struct addrInfo containing Internet (socket) address for connection, using bind() or connect()
  // node: specify a host
  // service: specify a service
  // hints: a hint for required socket address with specified features

  // no AI_PASSIVE               => socket addresses suitable for connect(), sendto(), sendmsg()        --- *
  // no AI_PASSIVE & node = NULL => socket addresses = loop back address                                --- *
  // AI_PASSIVE    & node = NULL => socket addresses suitable in a server to bind() a socket & accept() 
  if ((err = getaddrinfo(NULL/*this node*/, port, &hints, &server)))
    error(1, 0, "getaddrinfo: %s", gai_strerror(err));

  // ntohs()     : network byte-order to host byte-order
  // inet_ntoa() : convert the Internet address from (struct in_addr) to (string) 
  printf("%s: created addrinfo of family, type, protocol, flag, address, port = %d, %d, %d, %d, %s, %d \n"
         "(hints family, type, protocol, flag, address, port = %d, %d, %d, %d, %s, %d)\n", 
          PROG, server->ai_family, server->ai_socktype, server->ai_protocol, server->ai_flags,
          inet_ntoa(((struct sockaddr_in*)(server->ai_addr))->sin_addr),
          ntohs(((struct sockaddr_in*)(server->ai_addr))->sin_port),
          hints.ai_family, hints.ai_socktype, hints.ai_protocol, hints.ai_flags,"0",0
          // inet_ntoa(((struct sockaddr_in*)(hints.ai_addr))->sin_addr),
          // ntohs(((struct sockaddr_in*)(hints.ai_addr))->sin_port)
          );

  // socket(): create an endpoint for connection (viewed as a file, using file descriptor)
  // domain:    protocol family - ai_family (e.g. IPv4, IPv6, etc.), definning the packet
  // type:      socket type - ai_socktype (e.g. SOCK_STREAM, etc.), defining communication type (byte-stream, connectionless, ...)
  // protocol:  a specific protocol in the specified protocol family
  //              => normally within a given "protocol family", only a single protocol exists to support a particular "socket type" 
  //              => 0 in this normal case

  // create client-end socket, which must have the same family, type & protocol as the server (described in "server")
  sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (sock < 0)
    resourceError(sock, "socket");
  printf("%s: socket created=%d\n", PROG, sock);

  // bind()      : bind the socket to an Internet address (local address & port), described by ai_addr & ai_addrlen
  //                => its format defined by protocol in sock
  if ((err = bind(sock, server->ai_addr, server->ai_addrlen)) != 0)
    resourceError(err, "bind");

  // listen(): mark (sock) as a passive socket => suitable for accept(), with a pending connection queue of max_len = backlog(=1)
  if ((err = listen(sock, 1)) != 0)
    resourceError(err, "listen");

  // getsockname(): return the current socket (sock) address info (socket address & its len) into corresponding buffer
  addrlen = sizeof(serverAddr);
  getsockname(sock, (struct sockaddr *) &serverAddr, &addrlen);
  printf("%s: successfully bound at address %s and listening on port %d\n", PROG, inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

  // accept()    : used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET), potentially blocking (waiting for a connection)
  //                => create a new socket on connection (hot-standby)
  //                => accept(sock, NULL, NULL) if not interested in client details
  addrlen = sizeof(clientAddr);
  sock2 = accept(sock, (struct sockaddr *) &clientAddr, &addrlen);
  if (sock2 < 0)
    resourceError(sock2, "accept");
  printf("%s: received connection from %s, now on port %d\n", PROG, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

  // recv():    receiver upto (sizeof(buf)) bytes from (sock) into (buf), with flage 0 (no flag)
  // send():    send (nbytes) chars from (bufr) into (sock), with flag 0 (no flag)
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


