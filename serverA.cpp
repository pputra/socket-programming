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

#define MYPORT "21444"
#define HOST_NAME "localhost"
#define MAXBUFLEN 100
#define BOOT_UP_MESSAGE "The Server A is up and running using UDP on port 21444\n"
#define MAP_FILE_NAME "map.txt"

struct Edge {
  int dest;
  int len;
};

struct Map {
  int prop_speed;
  int trans_speed;
  map<int, vector<Edge> > maps;
  int num_edges;
  int num_vertices;
};

map<string, Map> maps;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

vector<string> read_file() {
  vector<string> inputs;
  string line;
  ifstream file(MAP_FILE_NAME);
  if (file.is_open()) {
    while (getline(file, line)) {
      inputs.push_back(line);
    }
  }

  file.close();

  return inputs;
}

void construct_maps() {
  vector<string> inputs = read_file();

  for (int i = 0; i < inputs.size(); i++) {
    string input = inputs[i];
    int num_edges = 0;

    if (isalpha(input[0])) {
      string mapId = input;
      Map map;

      map.prop_speed = atoi(inputs[++i].c_str());
      map.trans_speed = atoi(inputs[++i].c_str());

      i++;
      while (true) {
        if (i+1 >= inputs.size() || isalpha(inputs[i+1][0])) {
          break;
        }
        cout << inputs[i] << endl;

        num_edges++;
        i++;
      }
      map.num_edges = num_edges;
      maps[mapId] = map;
      cout << mapId << endl;
      cout << num_edges << endl;
      cout << "-----" << endl;
    }
  }      
}

int main(void) {
  construct_maps();

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

    // avoid namespace conflict with std
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

  while (true) {
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    printf("listener: got packet from %s\n",
    inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
  }
}