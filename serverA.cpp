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
#define BOOT_UP_MESSAGE "The Server A is up and running using UDP on port 21444\n\n"
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

void print_maps_info() {
  cout << "The Server A has constructed a list of " <<  maps.size() << " maps:\n" << endl;
  cout << "------------------------------------------" << endl;
  cout << "Map ID Num Vertices Num Edges" << endl;
  cout << "------------------------------------------" << endl;

  map<string, Map>::iterator it;

  for (it = maps.begin(); it != maps.end(); it++) {
    cout << it->first << "       " << it->second.num_vertices << "             " << it->second.num_edges << endl;
  }

  cout << "------------------------------------------" << endl;
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

void append_edges(int start_node, int dest_node, int edge_len, map<int, vector<Edge> > &edge_map) {
  Edge edge;
  edge.dest = dest_node;
  edge.len = edge_len;

  vector<Edge> prev_edges;

  if (edge_map.find(start_node) != edge_map.end()) {
    prev_edges = edge_map[start_node];
  }

  prev_edges.push_back(edge);
  edge_map[start_node] = prev_edges;
}

void parse_edges(string edge_str, map<int, vector<Edge> > &edge_map) {
  int start_node;
  int dest_node;
  int edge_len;

  int stop_index = edge_str.find_first_of(" ");
  
  start_node = atoi(edge_str.substr(0, stop_index).c_str());

  edge_str = edge_str.substr(stop_index + 1);
  stop_index = edge_str.find_first_of(" ");
  dest_node = atoi(edge_str.substr(0, stop_index).c_str());

  edge_str = edge_str.substr(stop_index + 1);
  edge_len = atoi(edge_str.c_str());

  append_edges(start_node, dest_node, edge_len, edge_map);
  append_edges(dest_node, start_node, edge_len, edge_map);
}

void construct_maps() {
  vector<string> inputs = read_file();

  for (int i = 0; i < inputs.size(); i++) {
    string input = inputs[i];
    int num_edges = 1;

    if (isalpha(input[0])) {
      Map curr_map;
      string mapId = input;
      map<int, vector<Edge> > edge_map;

      curr_map.prop_speed = atoi(inputs[++i].c_str());
      curr_map.trans_speed = atoi(inputs[++i].c_str());

      i++;
      while (true) {
        if (i+1 >= inputs.size() || isalpha(inputs[i+1][0])) {
          break;
        }
        string curr_edge_str = inputs[i];

        parse_edges(curr_edge_str, edge_map);

        num_edges++;
        i++;
      }
      curr_map.num_edges = num_edges;
      curr_map.maps = edge_map;
      curr_map.num_vertices = edge_map.size();
      maps[mapId] = curr_map;
    }
  }      
}

string get_shortest_path(string map_id, string start_index) {
  return "shortest map with id:" + map_id + " start index: " + start_index;
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
  print_maps_info();

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

    string message = string(buf);
    int parse_index = message.find_first_of(" ");

    string map_id = message.substr(0, parse_index);
    string start_index = message.substr(parse_index + 1);

    string map = get_shortest_path(map_id, start_index);
    sendto(sockfd, map.c_str(), strlen(map.c_str()), 0, (struct sockaddr *)&their_addr, addr_len);
  }
}