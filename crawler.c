/* simple TCP client for comp3310
 * Peter Strazdins, RSCS ANU, 04/14; updated 03/18
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>     // strstr, strcmp, ...
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>     // getaddrinfo(), getnameinfo()
#include <unistd.h>    // close(), sleep()
#include <error.h>     // error()
#include <arpa/inet.h> // inet_ntoa(), ntohs()
#include <math.h>
#include <time.h>      // strptime()
#include <ctype.h>     // toupper()



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

// The number of pages
// The largest page (and size)
// The most-recently modified page (and its date/time)
// A list of invalid pages (not) found (404)
// A list of redirected pages found (30x) and where they redirect to

#define BUFLEN 4096
#define PROG "client"

#define MIN(a, b) ((a) <= (b)? (a): (b))

void resourceError(int status, char *caller);

void send_to_socket(int sock, const char* request, int print);

typedef struct Data_Buffer_{
  char* restrict buffer;
  char*          cur_pos;
  unsigned int   buf_size;
  unsigned int   buf_exceed_cnt;
  unsigned int   used_buf;
} Data_Buffer;

Data_Buffer* init_data_buffer();
Data_Buffer* init_data_buffer_with_size(const unsigned int buf_size);
void free_data_buffer(Data_Buffer* data_buffer);
void increase_data_buffer(Data_Buffer* data_buffer);
void copy_string_into_buffer(Data_Buffer* data_buffer, const char* const temp_buffer);
void receive_from_socket(int sock_fd, Data_Buffer* const data_buffer, const struct addrinfo* const server);

typedef struct Domain_List_{
  char         domain [BUFLEN];
  char         port   [BUFLEN];
  char         page   [BUFLEN];
  char         response [4];
  unsigned int size;
  struct Domain_List_ * next;
} Domain_List;

Domain_List* create_domain_list_head();
int init_next_domain(Domain_List* head, Domain_List* cur_domain, const char* domain, const char* port, const char* page);
Domain_List* free_cur_domain(Domain_List* const head, Domain_List* cur_domain);

void record_domain_info(const Data_Buffer* const data_buffer, Domain_List* const cur_domain);
int get_http_response(Data_Buffer* const data_buffer, Domain_List* cur_domain);
void search_url(Data_Buffer* const data_buffer, Domain_List* head, Domain_List* cur_domain);

int is_same_domain(const Domain_List* const L, const Domain_List* const R);
unsigned int count_pages(const Domain_List* const head);

void print_302_pages(const Domain_List* const head);
void print_404_pages(const Domain_List* const head);
void print_largest_page(const Domain_List* const head);
void print_most_recent_modified_page(const Domain_List* const head);
void print_all_links(const Domain_List* const head);

struct addrinfo * resolve_domain(const char* domain, const char* port, struct addrinfo *server, int debugging);

int main(int argc, char* argv[]) {

  // Usage:
  // `./clientAddr init_domain init_port init_page` to start crawling from website: init_domain:init_port/init_page
  // to use other request format, sepcify the format in the 4th argument.
  const char* const init_domain = (argc == 3)? argv[1] : "3310exp.hopto.org";
  const char* const init_port   = (argc == 3)? argv[2] : "9780";
  const char* const init_page   = (argc == 3)? argv[3] : "";
  const char* const format      = (argc == 4)? argv[4] : "GET /%s HTTP/1.0\r\n\r\n";

  int err = 0;

  // Initialize domain list
  Domain_List* head = create_domain_list_head();
  Domain_List* cur_domain = head;
  init_next_domain(head, cur_domain, init_domain, init_port, init_page);
  cur_domain = cur_domain->next;

  // DFS
  int cnt = 0;
  while (cur_domain != NULL && cnt < 100)
  {
    int nbytes              = 0;
    int sock                = 0;   // client socket
    struct addrinfo *server = NULL;
    
    err = 0;

    // resolve domain
    server = resolve_domain(cur_domain->domain, cur_domain->port, server, 0);

    // create the client's end socket
    do {

      // get the socket, which must have the same family, type & protocol
      sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);

      if (sock < 0) continue;
      else if (server->ai_socktype != SOCK_STREAM) // UDP
      {
        printf("using UDP\n");
        break;
      }
      else // TCP
      {
        // --- debug output : ensure the sock created successfully ---
        // printf("%s: socket created=%d\n", PROG, sock);

        // --- debug output : output the host info & server info ---
        // converts a socket address to a corresponding host and service (protocol-independent)
        // char serverinfo[BUFLEN], hostinfo[BUFLEN];
        // getnameinfo(server->ai_addr, server->ai_addrlen, hostinfo, sizeof(hostinfo), serverinfo, sizeof(serverinfo), 0);
        // printf("try connecting to %s on port %s\n", hostinfo, serverinfo);

        // attempt to connect to the server (when using TCP)
        if ((err = connect(sock, server->ai_addr, server->ai_addrlen)) < 0)
          close(sock);
        else break;
      }
      
      server = server->ai_next;
    
    } while (server != NULL);
    if (server == NULL)
      resourceError(err, "connect");

    // --- debug output : ensure the right server (socket's address) is connected ---
    // get the socket's address into corresponding variables
    // socklen_t addrlen;
    // struct sockaddr_in clientAddr;  // info of sock; see /usr/include/linux/in.h
    // addrlen = sizeof(clientAddr);
    // getsockname(sock, (struct sockaddr *) &clientAddr, &addrlen);
    // printf("%s: client at %s connected to server, using port %d\n", PROG, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
    printf("connected onto %s:%s/%s\n", cur_domain->domain, cur_domain->port, cur_domain->page);

    // construct the request to fetch the page
    char fetch_request[BUFLEN];
    nbytes = snprintf(fetch_request, BUFLEN, format, cur_domain->page); // '\0' ends ensured
    if (nbytes > sizeof(fetch_request) || nbytes < 0) resourceError(nbytes, "fetch_request");

    // send request to server
    if (server->ai_socktype == SOCK_STREAM)
      nbytes = send(sock, fetch_request, strlen(fetch_request), 0);                                        // TCP
    else 
      nbytes = sendto(sock, fetch_request, strlen(fetch_request), 0, server->ai_addr, server->ai_addrlen); // UDP
    if (nbytes < 0) resourceError(nbytes, "send");

    // --- debug output : ensure the right request is sent ---
    // printf("%s: sent message (%d bytes, original size %ld):\n", PROG, nbytes, strlen(fetch_request));
    // printf("%s\n", fetch_request);

    // initializa data buffer
    Data_Buffer* data_buffer = init_data_buffer();

    // receive packets from server (using the socket)
    receive_from_socket(sock, data_buffer, server);

    // close current connection
    close(sock);
    freeaddrinfo(server);

    // record domain info from buffer
    record_domain_info(data_buffer, cur_domain);
    
    // check response from server
    int retry = get_http_response(data_buffer, cur_domain);

    // add URLs in current page into domain list
    search_url(data_buffer, head, cur_domain);


    printf("full messages: \n\n%s\n", data_buffer->buffer);


    // move to next domain if not to retry
    if (!retry)
    {
      cur_domain = cur_domain->next;
      cnt ++;
      printf("----------------------------------------\n\n\n\n\n");
    }
    else printf("retry...\n");

    // clean up current data buffer
    free_data_buffer(data_buffer);

    // ensure 2 sec sleep before the next call
    sleep(3);
  }

  printf("%s: web crawler stopped, giving out result:\n", PROG);

  printf("In total %d unique pages encountered.\n", count_pages(head));
  printf("\n");

  print_largest_page(head);
  printf("\n");

  print_most_recent_modified_page(head);
  printf("\n");

  print_302_pages(head);
  printf("\n");

  print_404_pages(head);

  return 0;
} //main()

// translate errno into humand readable error
void resourceError(int status, char *caller)
{
  printf("%s: resource error status=%d\n", PROG, status);
  perror(caller);
  exit(2);
}

// wrapper for send(2) => print info for debug use
void send_to_socket(int sock, const char* request, int print)
{
  int nbytes = send(sock, request, sizeof(request), 0);
  if (nbytes < 0) 
    resourceError(nbytes, "send");
  if (print)
  {
    printf("%s: sent message (%d bytes, original size %ld):\n", PROG, nbytes, sizeof(request));
    printf("%s\n", request);    
  }
}

// initialize data buffer (of initial size BUFLEN)
Data_Buffer* init_data_buffer()
{
  return init_data_buffer_with_size(BUFLEN);
}

// initialize data buffer with specified size
Data_Buffer* init_data_buffer_with_size(const unsigned int buf_size)
{
  Data_Buffer*  data_buffer   = (Data_Buffer*)malloc(sizeof(Data_Buffer));
  data_buffer->buf_size       = buf_size;
  data_buffer->buffer         = (char*)malloc(data_buffer->buf_size); // allocate mem
  memset(data_buffer->buffer, 0, data_buffer->buf_size);              // set to 0
  data_buffer->cur_pos        = data_buffer->buffer;                  // point to start
  data_buffer->buf_exceed_cnt = 0;                                    // no exceed histroy
  data_buffer->used_buf       = 0;                                    // no used

  return data_buffer;
}

// depose the buffer => free the memory
void free_data_buffer(Data_Buffer* data_buffer)
{
  free(data_buffer->buffer);
  free(data_buffer);
}

// increase the buffer & remain the contents
void increase_data_buffer(Data_Buffer* data_buffer)
{
  unsigned int new_buf_size = (int)pow(2, data_buffer->buf_exceed_cnt); // increase by 2^n, n = times of buffer get exceeded
  Data_Buffer* new_buf      = init_data_buffer_with_size(new_buf_size);

  assert(sizeof(data_buffer->buf_size) < sizeof(new_buf->buf_size));    // ensure the size increases

  // copy into new_buf
  int nbytes = snprintf(new_buf->buffer, new_buf->buf_size, "%s", data_buffer->buffer);
  if (nbytes < 0 || nbytes >= new_buf->buf_size) 
    resourceError(nbytes, "increase_data_buffer - copy failed");
  
  new_buf->cur_pos        = data_buffer->cur_pos;               
  new_buf->buf_exceed_cnt = data_buffer->buf_exceed_cnt;
  new_buf->used_buf       = data_buffer->used_buf;

  Data_Buffer* old_buf = data_buffer;
  data_buffer          = new_buf;

  free_data_buffer(old_buf);
  return;
}

// copy a given string into the structured buffer, with protection
void copy_string_into_buffer(Data_Buffer* data_buffer, const char* const temp_buffer)
{
  // ensure '\0' ends
  unsigned int size = strlen(temp_buffer)+1;
  while (size >= data_buffer->buf_size - data_buffer->used_buf)
  {
    data_buffer->buf_exceed_cnt++;
    increase_data_buffer(data_buffer);
  }

  // copy into buffer & update
  int nbytes = snprintf(data_buffer->cur_pos, data_buffer->buf_size - data_buffer->used_buf, "%s", temp_buffer);
  if (nbytes < 0 || nbytes >= data_buffer->buf_size - data_buffer->used_buf)
    resourceError(nbytes, "copy_string_into_buffer - copy failed");
  
  data_buffer->cur_pos += nbytes;
  data_buffer->used_buf += nbytes;
}

// receive packet from server into the given structured buffer
void receive_from_socket(int sock_fd, Data_Buffer* const data_buffer, const struct addrinfo* const server)
{
    // receive loops (might receive multiple packets)
    while(1){

      // initialize temp buffer
      char temp_buffer[BUFLEN];
      memset(temp_buffer, 0, sizeof(temp_buffer)); 

      // receive reply from server
      int nbytes = 0;
      if (server->ai_socktype == SOCK_STREAM) // TCP
        nbytes = recv(sock_fd, temp_buffer, sizeof(temp_buffer), 0);
      else
      {
        // UDP
        unsigned int addrlen = server->ai_addrlen;
        nbytes = recvfrom(sock_fd, temp_buffer, sizeof(temp_buffer), 0, server->ai_addr, &(addrlen));
      }

      if (nbytes < 0) resourceError(nbytes, "recv");
      else if (nbytes == 0) break;
      else
      {
        // check against buffer overflow => ensure '\0' ends
        assert(nbytes < sizeof(temp_buffer));
        
        // copy into data_buffer
        copy_string_into_buffer(data_buffer, temp_buffer);
      }
    }
}

int convert_month(char* const mon)
{
  char *c = mon;
  while((*c) != '\0')
    *c = toupper((unsigned char) *c);

  char *all_mon = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
  c = strstr(all_mon, mon);
  return (int)(c - all_mon)/3;
}

// find the last modified data of current page
// void find_last_modified_date(const char* const buffer)
// {
//   struct tm *tm = NULL;
//   const char *time_string = strstr(buffer, "Last-Modified:");

//   printf("----------------\n");

//   if (time_string != NULL)
//   {
//     // Last-Modified: Mon(%a), 26(%d) Mar(%b) 2018(%Y) 03(%H):53(%M):08(%S) GMT
//     strptime(time_string, "Last-Modified: %a, %d %b %Y %H:%M:%S GMT", tm);
//     char time_arr[128];
//     strftime(time_arr, sizeof(time_arr), "found: : %a, %d %b %Y %H:%M:%S GMT", tm);
//     time_arr[sizeof(time_arr)-1] = '\0';
//     printf("%s\n", time_arr);
//   }

//   return;
// }

// find the last modified data of current page
void find_last_modified_date(const char* const buffer)
{
  struct tm time;
  memset(&time, 0, sizeof(time));
  int day;
  char month[4];
  int year;
  int hours;
  int min;
  int sec;

  month[3] = '\0';
  sscanf(buffer, "Last-Modified: %*[^,], %d %s %d %d:%d:%d GMT", &day, month, &year, &hours, &min, &sec);
  int mon = convert_month(month);
  time.tm_sec = sec; //  int seconds after the minute  0-60*
  time.tm_min = min; // int minutes after the hour  0-59
  time.tm_hour = hours; // int hours since midnight  0-23
  time.tm_mday = day; // int day of the month  1-31
  time.tm_mon  = mon; //int months since January  0-11
  time.tm_year = year; // int years since 1900   
  printf("last modified time %d %s %d %d:%d:%d GMT",day, month, year, hours, min, sec);
  return;
}


// record info (size, last-update) of current page (in the data buffer) into the current node in domain list
void record_domain_info(const Data_Buffer* const data_buffer, Domain_List* const cur_domain)
{
  cur_domain->size = data_buffer->used_buf;
  find_last_modified_date(data_buffer->buffer);
  return;
}


// create a head of a domain list
Domain_List* create_domain_list_head()
{
  Domain_List* head = (Domain_List*)malloc(sizeof(Domain_List));
  head->next = NULL;
  return head;
}

// ensure the domain not discoved before & insert it into the first position of the domain list 
// limit: 1. need to call BEFORE updating cur_domain (assuming current domain is discovered)
//        2. not checking duplicate links inside current page
int init_next_domain(Domain_List* head, Domain_List* cur_domain, const char* domain, const char* port, const char* page)
{
  // get IP address
  struct addrinfo *server = NULL;
  server = resolve_domain(domain, port, server, 0);
  if (server == NULL) // not successfully resolved => skip
    return 0;
    
  // copy IP address into buffer, '\0' ends ensured
  char domain_IP[BUFLEN]; 
  memset(domain_IP, 0, sizeof(domain_IP));
  int nbytes = snprintf(domain_IP, sizeof(domain_IP), "%s", inet_ntoa(((struct sockaddr_in*)(server->ai_addr))->sin_addr));
  if (nbytes > sizeof(domain_IP) || nbytes < 0) resourceError(nbytes, "init_next_domain - copy IP failed");

  // free resource
  freeaddrinfo(server);

  // check if the new domain is visited before
  int is_new = 1;
  Domain_List* past_domain = head->next;
  while (past_domain != NULL && past_domain != cur_domain->next)
  {
    if (!strcmp(past_domain->domain, domain_IP) && !strcmp(past_domain->port, port) && !strcmp(past_domain->page, page))
    {
      is_new = 0;
      break;
    }

    past_domain = past_domain->next;
  }

  // insert into the list if new website discovered
  printf("%s:%s/%s - IP: %s", domain, port, page, domain_IP);

  if (is_new)
  {
    Domain_List* next_domain = (Domain_List*)malloc(sizeof(Domain_List));

    // ensure enough space of domain, port, page
    if (strlen(domain_IP) >= BUFLEN || strlen(port) >= BUFLEN || strlen(page) >= BUFLEN)
    {
      printf("init_next_domain: len(domain_IP)=%ld / len(port)=%ld / len(page)=%ld exceed BUFLEN %d\n", 
              strlen(domain_IP), strlen(port), strlen(page), BUFLEN);
      printf("domain: %s, IP: %s, port: %s, page: %s\n", domain, domain_IP, port, page);
      exit(2);
    }

    // copy into fields
    snprintf(next_domain->domain, sizeof(next_domain->domain), "%s", domain_IP);
    snprintf(next_domain->port, sizeof(next_domain->port), "%s", port);
    snprintf(next_domain->page, sizeof(next_domain->page), "%s", page);

    // initialize fields
    memset(next_domain->response, 0, sizeof(next_domain->response));
    next_domain->size = 0;

    // insert into list
    next_domain->next = cur_domain->next;
    cur_domain->next  = next_domain;
  }
  else
    printf(" --- duplicate");
  
  printf("\n");

  return is_new;
}

// depose the current domain, returned the next domain
Domain_List* free_cur_domain(Domain_List* const head, Domain_List* cur_domain)
{
  assert(cur_domain != NULL);
  Domain_List* next_domain = cur_domain->next;
  Domain_List* last_domain = head;
  while(last_domain->next != cur_domain) last_domain = last_domain->next;

  last_domain->next = next_domain;

  // free(cur_domain->domain);
  // free(cur_domain->port);
  // free(cur_domain->page);
  // free(cur_domain->response);
  free(cur_domain);
  return next_domain;
}


// check if the request is successful & record the response & re-try if needed
int get_http_response(Data_Buffer* const data_buffer, Domain_List* cur_domain)
{
  const char* buffer = data_buffer->buffer;
  sscanf(buffer, "HTTP/%*d.%*d %s\n", cur_domain->response); // parse header
  cur_domain->response[3] = '\0';

  return !strcmp(cur_domain->response, "503");
}

// parse the returned data & find all urls in it & insert into domain list (if not duplicate)
void search_url(Data_Buffer* const data_buffer, Domain_List* head, Domain_List* cur_domain)
{
  const char* cur_pos = data_buffer->buffer;
  while ((cur_pos = strstr(cur_pos, "<a href=\"http://")) != NULL)
  {
    char domain [BUFLEN];
    char port   [BUFLEN];
    char page   [BUFLEN];
    char url    [2*BUFLEN];

    memset(domain, 0, sizeof(domain));
    memset(port,   0, sizeof(port));
    memset(page,   0, sizeof(page));
    memset(url,    0, sizeof(url));

    // get domain:port/page into url
    sscanf(cur_pos, "<a href=\"http://%[^\"]\">", url);
    cur_pos++;

    // page included in the url
    if (strstr(url, "/") != NULL)
      sscanf(url, "%[^:]:%[^/]/%[^\"]", domain, port, page);
    else
      sscanf(url, "%[^:]:%[^/]", domain, port);

    // try to insert into domain list
    init_next_domain(head, cur_domain, domain, port, page);
  }
}

void print_302_pages(const Domain_List* const head)
{
  printf("redirected pages: \n");
  int cnt = 0;
  Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    if (!strcmp(cur_domain->response, "302"))
    {
      Domain_List* next_domain = cur_domain->next;
      if (next_domain != NULL)
      {
        cnt ++;
        printf("%d: %s:%s/%s redirected to %s:%s/%s\n", 
                cnt, cur_domain->domain, cur_domain->port, cur_domain->page, next_domain->domain, next_domain->port, next_domain->page);
      }
    }
    cur_domain = cur_domain->next;
  }

  printf("In total, %d pages redirected\n", cnt);
  return;
}

void print_404_pages(const Domain_List* const head)
{
  printf("404 pages:\n");
  int cnt = 0;
  const Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    if (!strcmp(cur_domain->response, "404"))
    {
      cnt ++;
      printf("%d: %s:%s/%s page not found\n", cnt, cur_domain->domain, cur_domain->port, cur_domain->page);
    }
    cur_domain = cur_domain->next;
  }

  printf("In total, %d pages not found\n", cnt);
  return;
}

void print_largest_page(const Domain_List* const head)
{
  unsigned int max_size = 0;
  Domain_List* max_domain = NULL;

  Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    if (cur_domain->size > max_size) 
    {
      max_domain = cur_domain;
      max_size = cur_domain->size;
    }
    cur_domain = cur_domain->next;
  }

  assert(max_domain != NULL && max_size > 0);
  printf("The largest page is %s:%s/%s, with size of %d\n", max_domain->domain, max_domain->port, max_domain->page, max_size);
  return;
}

void print_most_recent_modified_page(const Domain_List* const head)
{
  return;
}

void print_all_links(const Domain_List* const head)
{
  const Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    printf("%s:%s/%s\n", cur_domain->domain, cur_domain->port, cur_domain->page);
    cur_domain = cur_domain->next;
  }
}


int is_same_domain(const Domain_List* const L, const Domain_List* const R)
{
  return (!strcmp(L->domain, R->domain) && !strcmp(L->port, R->port) && !strcmp(L->page, R->page));
}

unsigned int count_pages(const Domain_List* const head)
{
  unsigned int cnt = 0;
  const Domain_List* cur_domain = head->next;

  while(cur_domain != NULL)
  {
    // check duplicate domain => count them for only once
    unsigned int is_unique = 1;
    Domain_List* other_domain = cur_domain->next;

    while(other_domain != NULL)
    {
      if (is_same_domain(cur_domain, other_domain))
      {
        is_unique = 0;
        break;
      }
      other_domain = other_domain->next;
    }

    cnt += is_unique;
    cur_domain = cur_domain->next;
  }

  return cnt;
}

struct addrinfo * resolve_domain(const char* domain, const char* port, struct addrinfo *server, int debugging)
{
  // initialize hints
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); // use defaults unless overridden
  hints.ai_family = AF_UNSPEC;      // IPv4; for IPv6, use AF_INET6; AF_UNSPEC 
  hints.ai_socktype = SOCK_STREAM;  // SOCK_STREAM for TCP; SOCK_DGRAM for UDP => default to TCP
  hints.ai_protocol = 0;            // any protocol

  // --- debug output : ensure the right IP:port is expected to be resolved ---
  if (debugging)
    printf("%s: hints family, type, protocol, flag & given server, port = %d, %d, %d, %d, %s, %s\n", 
            PROG, hints.ai_family, hints.ai_socktype, hints.ai_protocol, hints.ai_flags, domain, port);

  // resolve server's addrinfo: on domain name, port, with following hints for connection type
  int err = 0;
  if ((err = getaddrinfo(domain, port, &hints, &server)))
  {
    if (err == EAI_NONAME) // domain resolution failed -> no such domain
    {
      printf("%s:%s failed to be resolved, skipped.\n", domain, port);
      return server;
    }
    else if (err == EAI_SERVICE)
    {
      hints.ai_socktype = SOCK_DGRAM; // switch to UDP & retry
      printf("switch to UDP.\n");
      if ((err = getaddrinfo(domain, port, &hints, &server)))
        error(1, 0, "getaddrinfo: %s", gai_strerror(err));
    }
    else // other failure
      error(1, 0, "getaddrinfo: %s", gai_strerror(err));
  }

  // --- debug output : ensure the right IP:port is resolved ---
  // ntohs()     : network byte-order to host byte-order
  // inet_ntoa() : convert the Internet address from (struct in_addr) to (string) 
  if (debugging)
    printf("%s: resolved addrinfo of family, type, protocol, flag, address, port = %d, %d, %d, %d, %s, %d \n",
            PROG, server->ai_family, server->ai_socktype, server->ai_protocol, server->ai_flags,
                  inet_ntoa(((struct sockaddr_in*)(server->ai_addr))->sin_addr), ntohs(((struct sockaddr_in*)(server->ai_addr))->sin_port));    
  return server;
}