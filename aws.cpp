#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <ctype.h>
#include <vector> 
#include<map>

using namespace std;

#define TCP_PORT "24444"  // the port users will be connecting to
#define HOST_NAME "localhost" // hostname
#define SERVER_A_PORT "21444" // serverA port
#define SERVER_B_PORT "22444" // serverB port
#define BACKLOG 10     // how many pending connections queue will hold
#define BOOT_UP_MESSAGE "The AWS is up and running.\n\n"

#define MAXDATASIZE 10000 // max number of bytes we can get at once

struct Node {
  int id;
  int dist;
  long double trans_time;
  long double prop_time;
  long double delay_time;
};

struct Paths {
  map<int, Node> node_map;
  long double trans_speed;
  long double prop_speed;
};

void sigchld_handler(int);
void *get_in_addr(struct sockaddr*);
vector<string> split_string_by_delimiter(string, string);
Paths request_shortest_path(string, string, string);
Paths create_paths(string);
void print_shortest_paths(Paths&);
void request_delays(string, Paths&);

int main(void) {
  int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes=1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(HOST_NAME, TCP_PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf(BOOT_UP_MESSAGE);

  while(1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family,
      get_in_addr((struct sockaddr *)&their_addr),
      s, sizeof s);
  
    // printf("server: got connection from %s\n", s);

    if (!fork()) { // this is the child process
      close(sockfd); // child doesn't need the listener
      char buf[MAXDATASIZE];
      
      if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
      }

      buf[numbytes] = '\0';
      
      string message = string(buf);

      vector<string> client_payloads = split_string_by_delimiter(message, " ");

      string map_id = client_payloads[0];
      string source_vertex_index = client_payloads[1];
      string file_size = client_payloads[2];

      cout << "The AWS has received map ID " + map_id + ", start vertex " + source_vertex_index + " and file size " + file_size + " from the client using TCP over port " + TCP_PORT;
      cout << endl;

      Paths paths = request_shortest_path(SERVER_A_PORT, map_id, source_vertex_index);

      request_delays(SERVER_B_PORT, paths);

      close(new_fd);
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }

  return 0;
}

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while(waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
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

Paths request_shortest_path(string destination_port, string map_id, string start_index) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  string message = map_id + " " + start_index;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(HOST_NAME, destination_port.c_str(), &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(1);
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
        perror("talker: socket");
        continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "aws: failed to create socket\n");
    exit(1);
  }

    if ((numbytes = sendto(sockfd, message.c_str(), strlen(message.c_str()), 0,
            p->ai_addr, p->ai_addrlen)) == -1) {
      perror("aws: sendto");
      exit(1);
    }

    freeaddrinfo(servinfo);

    cout << endl;
    cout << "the AWS has sent map ID and starting vertex to server A using UDP over port " + destination_port << endl;

    char buf[MAXDATASIZE];

    if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
        p->ai_addr, &p->ai_addrlen)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    buf[numbytes] = '\0';
    // printf("aws: packet contains \"%s\"\n", buf);

    Paths paths = create_paths(string(buf));

    print_shortest_paths(paths);

    close(sockfd);

    return paths;
}

Paths create_paths(string response) {
  Paths paths;
  vector<string> inputs = split_string_by_delimiter(response, ",");

  paths.prop_speed = stold(inputs[1]);
  paths.trans_speed = stold(inputs[2]);

  vector<string> nodes = split_string_by_delimiter(inputs[0], "-");

  for (int i = 0; i < nodes.size(); i++) {
    vector<string> inner_inputs = split_string_by_delimiter(nodes[i], " ");
    int id = atoi(inner_inputs[0].c_str());
    int dist = atoi(inner_inputs[1].c_str());

    Node node;
    node.id = id;
    node.dist = dist;

    paths.node_map[id] = node;
  }

  return paths;
}

void print_shortest_paths(Paths &paths) {
  cout << endl;
  cout << "The AWS has received shortest path from server A:" << endl;
  cout << "------------------------------------------" << endl;
  cout << "Destination Min Length" << endl;
  cout << "------------------------------------------" << endl;

  map<int, Node>::iterator it = paths.node_map.begin();

  while (it != paths.node_map.end()) {
    int id = it->first;
    int dist = it->second.dist;

    cout << to_string(id) << "               " << to_string(dist) << endl;

    it++;
  }

  cout << "------------------------------------------" << endl;
}

void request_delays(string destination_port, Paths &paths) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  string message = "hello server b";

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(HOST_NAME, destination_port.c_str(), &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "aws: failed to create socket\n");
    exit(1);
  }

  if ((numbytes = sendto(sockfd, message.c_str(), strlen(message.c_str()), 0,
          p->ai_addr, p->ai_addrlen)) == -1) {
    perror("aws: sendto");
    exit(1);
  }

  freeaddrinfo(servinfo);

  cout << endl;
  cout << "The AWS has sent path length, propagation speed and transmission speed to server B using UDP over port " + destination_port << endl;

  char buf[MAXDATASIZE];

  if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
      p->ai_addr, &p->ai_addrlen)) == -1) {
    perror("recvfrom");
    exit(1);
  }

  buf[numbytes] = '\0';

  cout << string(buf) << endl;
}