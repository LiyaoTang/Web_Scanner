/* Simple Web Crawler - u6142160 Liyao Tang

Readme.md:

The work is down based on the lab02 instructions and the assignment instruction.
The default website to start the crawling is the "3310exp.hopto.org:9780/".
Note that 'make run' command will only start from the default server. To start from a different website "domain:port/page", 
you can run the program as "./crawler domain port page". For example, to start from "3310exp.hopto.org:9780/20/25.html", 
run the complied program as "./crawler 3310exp.hopto.org 9780 20/25.html".

The crawler defaults to use TCP connection and will switch to UDP if EAI_SERVICE is reported in 'getaddrinfo()', but this function
is not tested against any server yet.

The crawler will receive all messages sent from server using a dynamic-array-like structure 'Data_Buffer', assuming the message in 
any single packet is smaller than BUFLEN (default to 4096) bytes. This function is designed in case that a large website arrives in 
multiple packets but is unfortunately not tested again.

The information of each web site is recored in a link list structure 'Domain_List' and each page will be only recorded once. Web pages 
are recorded only in their IP addresses to avoid duplicate, assuming that one web page's IP address can hardly change during the 
execution time of this simple crawler. That is, Two pages will be viewed as the same page if they lead to the same IP address during 
the execution of my crawler. While maintaining the list, crawler visits web pages in a DFS style.

The required result will be printed at the end and some intermediate results will be printed.
To be more specific, the required number of page is counted as unique IP addresses ever successfully resolved by the crawler, 
including 50x redirected pages and 404 not-found pages.
The largest page is the page with the longest content length, instead of the whole received message. 
The most-recently modified page is the page with largest Last-Modified time which is stored in type 'struct tm' and is converted 
into type 'time_t' using 'mk_time()' in comparison.

Finally, the crawler is built and tested successfully on the lab machine against the friendly test server.

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

#define BUFLEN 4096
#define PROG "web crawler"

#define MIN(a, b) ((a) <= (b)? (a): (b))

void resourceError(int status, char *caller);

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
  struct tm    last_modified_time;
  struct Domain_List_ * redirected_page;  
  struct Domain_List_ * next;
} Domain_List;

Domain_List* create_domain_list_head();
int init_next_domain(Domain_List* head, Domain_List* cur_domain, const char* domain, const char* port, const char* page);
Domain_List* free_cur_domain(Domain_List* const head, Domain_List* cur_domain);

void record_domain_info(const Data_Buffer* const data_buffer, Domain_List* const cur_domain, Domain_List* const head);

unsigned int count_pages(const Domain_List* const head);

void print_redirected_pages(const Domain_List* const head);
void print_404_pages(const Domain_List* const head);
void print_largest_page(const Domain_List* const head);
void print_most_recent_modified_page(const Domain_List* const head);
void print_all_links(const Domain_List* const head);

struct addrinfo * resolve_domain(const char* domain, const char* port, struct addrinfo *server, int debugging);

int main(int argc, char* argv[]) {

  // Usage:
  // `./clientAddr init_domain init_port init_page` to start crawling from website: init_domain:init_port/init_page
  // to use other request format, sepcify the format in the 4th argument.
  const char* const init_domain = (argc == 4)? argv[1] : "3310exp.hopto.org";
  const char* const init_port   = (argc == 4)? argv[2] : "9780";
  const char* const init_page   = (argc == 4)? argv[3] : "";
  const char* const format      = (argc == 5)? argv[4] : "GET /%s HTTP/1.0\r\n\r\n";
  int err = 0;

  // Initialize domain list
  printf("starting from: ");
  Domain_List* head = create_domain_list_head();
  Domain_List* cur_domain = head;
  init_next_domain(head, cur_domain, init_domain, init_port, init_page);
  cur_domain = cur_domain->next;
  printf("\n----------------------------------------\n\n");

  // DFS
  while (cur_domain != NULL)
  {
    int nbytes              = 0;
    int sock                = 0;   // client socket
    struct addrinfo *server = NULL;
    
    err = 0;

    // resolve domain
    server = resolve_domain(cur_domain->domain, cur_domain->port, server, 0);

    // create the client's end socket & connect if using TCP
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
    if (server == NULL) resourceError(err, "connect");

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

    // debug output : print out the full message received from server ---  
    // printf("full messages: \n\n\n%s\n", data_buffer->buffer);

    // record domain info from buffer
    record_domain_info(data_buffer, cur_domain, head);

    // move to next domain if not to retry
    if (strcmp(cur_domain->response, "503"))
    {
      cur_domain = cur_domain->next;
      printf("\n----------------------------------------\n\n");
    }
    else printf("retry on 503...\n");

    // clean up current data buffer
    free_data_buffer(data_buffer);

    // ensure 2 sec sleep before the next call
    sleep(3);
  }

  printf("%s: stopped crawling, giving out result:\n\n", PROG);
  printf("\n");

  printf("In total %d unique pages encountered.\n", count_pages(head));
  printf("\n");

  print_largest_page(head);
  printf("\n");

  print_most_recent_modified_page(head);
  printf("\n");

  print_redirected_pages(head);
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

// create a head of a domain list
Domain_List* create_domain_list_head()
{
  Domain_List* head = (Domain_List*)malloc(sizeof(Domain_List));
  head->next = NULL;
  return head;
}

// initialize a new domain with given domain, port & page
Domain_List* new_domain(const char* domain, const char* port, const char* page)
{
  Domain_List* new_domain = (Domain_List*)malloc(sizeof(Domain_List));

  // ensure enough space of domain, port, page
  if (strlen(domain) >= sizeof(new_domain->domain) || strlen(port) >= sizeof(new_domain->port) || strlen(page) >= sizeof(new_domain->page))
  {
    printf("new domain: len(domain_IP)=%ld / len(port)=%ld / len(page)=%ld exceed corresponding size of %ld %ld %ld\n", 
            strlen(domain), strlen(port), strlen(page), sizeof(new_domain->domain), sizeof(new_domain->port), sizeof(new_domain->page));

    printf("domain: %s, port: %s, page: %s\n", domain, port, page);
    exit(2);
  }

  // copy into fields
  snprintf(new_domain->domain, sizeof(new_domain->domain), "%s", domain);
  snprintf(new_domain->port, sizeof(new_domain->port), "%s", port);
  snprintf(new_domain->page, sizeof(new_domain->page), "%s", page);

  // initialize fields
  memset(new_domain->response, 0, sizeof(new_domain->response));
  memset(&(new_domain->last_modified_time), 0, sizeof(new_domain->last_modified_time));
  new_domain->size = 0;
  new_domain->redirected_page = NULL;
  new_domain->next = NULL;

  return new_domain;
}

// ensure the domain not discoved before & insert it into the domain list
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

  // check if the given domain is visited before
  int is_new = 1;
  Domain_List* past_domain = head->next;
  while (past_domain != NULL)
  {
    if (!strcmp(past_domain->domain, domain_IP) && !strcmp(past_domain->port, port) && !strcmp(past_domain->page, page))
    {
      is_new = 0;
      break;
    }
    past_domain = past_domain->next;
  }

  // insert into the list if new domain discovered
  printf("%s:%s/%s - IP: %s", domain, port, page, domain_IP);

  if (is_new)
  {
    // create new domain, using IP address
    Domain_List* next_domain = new_domain(domain_IP, port, page);

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

  free(cur_domain->redirected_page);
  free(cur_domain);
  return next_domain;
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

// convert month 3-letter abbr into int (0-11)
int convert_month(char* const mon)
{
  for (char*c = mon; (*c) != '\0'; c++)
    *c = toupper((unsigned char) *c);

  char *all_mon = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
  return (int)(strstr(all_mon, mon) - all_mon)/3;
}

// find the last modified data of current page
struct tm find_last_modified_date(const char* const buffer)
{
  struct tm time;
  int day   = 0;
  char month[4];
  int year  = 0;
  int hours = 0;
  int min   = 0;
  int sec   = 0;

  memset(&time, 0, sizeof(time));
  memset(month, '\0', sizeof(month));


  char* last_modified_line = strstr(buffer, "Last-Modified");
  if (last_modified_line != NULL)
  {
    sscanf(last_modified_line, "Last-Modified: %*[^,], %d %[A-Za-z] %d %d:%d:%d GMT", &day, month, &year, &hours, &min, &sec);

    int mon = convert_month(month);
    time.tm_sec  = sec; //  int seconds after the minute  0-60*
    time.tm_min  = min; // int minutes after the hour  0-59
    time.tm_hour = hours; // int hours since midnight  0-23
    time.tm_mday = day; // int day of the month  1-31
    time.tm_mon  = mon; //int months since January  0-11
    time.tm_year = year; // int years since 1900   
  }

  printf("last modified time %d %d %d %d:%d:%d GMT\n",time.tm_mday, time.tm_mon+1, time.tm_year, time.tm_hour, time.tm_min, time.tm_sec);
  return time;
}

unsigned int find_content_length(const char* const buffer)
{
  unsigned int content_length = 0;
  char* last_modified_line = strstr(buffer, "Content-Length");
  if (last_modified_line != NULL)
  {
    sscanf(last_modified_line, "Content-Length: %d", &content_length);
  }

  printf("content length: %d\n", content_length);
  return content_length;
}

// check if the request is successful & record the response
void record_http_response(const Data_Buffer* const data_buffer, Domain_List* const cur_domain)
{
  const char* buffer = data_buffer->buffer;
  sscanf(buffer, "HTTP/%*d.%*d %s\n", cur_domain->response); // parse header
  cur_domain->response[3] = '\0';
	printf("response: %s\n", cur_domain->response);
}

// find all urls in returned message & insert into domain list & into the redirected pate if necessary
void search_url(const Data_Buffer* const data_buffer, Domain_List* head, Domain_List* cur_domain)
{
  printf("finding urls...\n");

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

    // try to insert into the list
    init_next_domain(head, cur_domain, domain, port, page);
    if (cur_domain->response[0] == '3' && cur_domain->response[1] == '0') // record the redirected page if 30x
    {
      assert(cur_domain->redirected_page == NULL);
      cur_domain->redirected_page = new_domain(domain, port, page);
    }
  }
}

// record info (size, last-update) of current page (in the data buffer) into the current node in domain list
void record_domain_info(const Data_Buffer* const data_buffer, Domain_List* const cur_domain, Domain_List* const head)
{
  assert(cur_domain != NULL && head != NULL);
  record_http_response(data_buffer, cur_domain);                                  // record HTTP response
  cur_domain->size = find_content_length(data_buffer->buffer);                    // record size
  cur_domain->last_modified_time = find_last_modified_date(data_buffer->buffer);  // record last modified time
  search_url(data_buffer, head, cur_domain);                                      // serach url & insert into correct places

  return;
}

void print_redirected_pages(const Domain_List* const head)
{
  printf("redirected pages: \n");
  int cnt = 0;
  Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    if (cur_domain->response[0] == '3' && cur_domain->response[1] == '0')
    {
      assert(cur_domain->redirected_page != NULL);
      cnt ++;
      printf("%d: %s:%s/%s redirected to %s:%s/%s\n", 
              cnt, cur_domain->domain, cur_domain->port, cur_domain->page, 
              cur_domain->redirected_page->domain, cur_domain->redirected_page->port, cur_domain->redirected_page->page);
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
  printf("The largest page is %s:%s/%s, with size of %d bytes (header excluded)\n", max_domain->domain, max_domain->port, max_domain->page, max_size);
  return;
}

void print_most_recent_modified_page(const Domain_List* const head)
{
  struct tm recent_time;
  memset(&recent_time, 0, sizeof(recent_time));
  recent_time.tm_year = 0;

  time_t recent_t = mktime(&recent_time);
  Domain_List* recent_domain = NULL;

  Domain_List* cur_domain = head->next;
  while(cur_domain != NULL)
  {
    time_t last_modified_t = mktime(&(cur_domain->last_modified_time));

    if (difftime(last_modified_t, recent_t) > 0) // last_modified_t later than recent_t
    {
      recent_t = last_modified_t;
      recent_time = cur_domain->last_modified_time;
      recent_domain = cur_domain;
    }
    cur_domain = cur_domain->next;
  }

  assert(recent_domain != NULL);

  printf("Most resent modified page: %s:%s/%s, modified in D/M/Y %d/%d/%d, h/m/s %d:%d:%d GMT\n", 
          recent_domain->domain, recent_domain->port, recent_domain->page, 
          recent_time.tm_mday, recent_time.tm_mon, recent_time.tm_year, recent_time.tm_hour, recent_time.tm_min, recent_time.tm_sec);
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

unsigned int count_pages(const Domain_List* const head)
{
  unsigned int cnt = 0;
  const Domain_List* cur_domain = head->next;

  while(cur_domain != NULL)
  {
    cnt ++;
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
