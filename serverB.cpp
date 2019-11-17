#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <sys/wait.h>

#include <iostream>
#include <fstream>
#include <string>
#include <ctype.h>
#include <map>
#include <vector> 

using namespace std;

#define MYPORT "22444"
#define HOST_NAME "localhost"
#define MAXBUFLEN 100
#define BOOT_UP_MESSAGE "The Server B is up and running using UDP on port 22444\n"

vector<string> split_string_by_delimiter(string, string);
void *get_in_addr(struct sockaddr*);

int main(void) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  // set hints to be empty
  memset(&hints, 0, sizeof hints);
  // use either IPv4 or IPv6
  hints.ai_family = AF_UNSPEC;
  // set socket type to udp
  hints.ai_socktype = SOCK_DGRAM;
  // fill ip automatically
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(HOST_NAME, MYPORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      perror("listener: socket");
      continue;
    }

    if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
  }

  freeaddrinfo(servinfo);

  printf(BOOT_UP_MESSAGE);

  addr_len = sizeof their_addr;

  while (1) {
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    // printf("listener: got packet from %s\n",
    // inet_ntop(their_addr.ss_family,
    //     get_in_addr((struct sockaddr *)&their_addr),
    //     s, sizeof s));
    // printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);

    string response = "delay result";
    sendto(sockfd, response.c_str(), strlen(response.c_str()), 0, (struct sockaddr *)&their_addr, addr_len);
  }
}

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

vector<string> split_string_by_delimiter(string input, string delimiter) {
  vector<string> strings;
  int i = input.find_first_of(delimiter);

  while (i != string::npos) {
    string parsed_string = input.substr(0, i);
    strings.push_back(parsed_string);
    input = input.substr(i+1);
    i = input.find_first_of(delimiter);
  }

  if (input.size() > 0) strings.push_back(input);
  return strings;
}