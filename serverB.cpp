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
#include <cmath>

using namespace std;

#define MYPORT "22444"
#define HOST_NAME "localhost"
#define MAXBUFLEN 100
#define BOOT_UP_MESSAGE "The Server B is up and running using UDP on port 22444\n"

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
  long long file_size;
};

vector<string> split_string_by_delimiter(string, string);
void *get_in_addr(struct sockaddr*);
Paths create_paths(string);
void print_requested_paths_data(Paths&);
void calculate_delay(Paths&);
long double calculate_transmission_time(long double, long long);
long double calculate_propagation_time(long double, int);
void print_calculations_result(Paths&);
string to_string_decimal_place(long double, int);
string create_response(Paths&);
void print_success_message();

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

  printf(BOOT_UP_MESSAGE);

  addr_len = sizeof their_addr;

  while (1) {
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    buf[numbytes] = '\0';
  
    Paths paths = create_paths(buf);

    print_requested_paths_data(paths);
    calculate_delay(paths);
    print_calculations_result(paths);
    string result = create_response(paths);
    sendto(sockfd, result.c_str(), strlen(result.c_str()), 0, (struct sockaddr *)&their_addr, addr_len);
    print_success_message();
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

Paths create_paths(string response) {
  Paths paths;
  vector<string> inputs = split_string_by_delimiter(response, ",");

  paths.file_size = (stoll(inputs[1]));
  paths.prop_speed = stold(inputs[2]);
  paths.trans_speed = stold(inputs[3]);

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

void print_requested_paths_data(Paths &paths) {
  cout << endl;
  cout << "The Server B has received data for calculation:" << endl;
  cout << "* Propagation speed: "  <<  to_string(paths.prop_speed)  << " km/s;" << endl;
  cout << "* Transmission speed: "  <<  to_string(paths.trans_speed)  << " Bytes/s;" << endl;

  map<int, Node>::iterator it = paths.node_map.begin();

  while (it != paths.node_map.end()) {
    int id = it->first;
    int dist = it->second.dist;
    cout << "Path length for destination " << to_string(id) << ": " << to_string(dist) << endl;

    it++;
  }
}

void calculate_delay(Paths &paths) {
  map<int, Node>::iterator it = paths.node_map.begin();
  long long file_size = paths.file_size;
  long double trans_speed = paths.trans_speed;
  long double prop_speed = paths.prop_speed;
  while (it != paths.node_map.end()) {
    it->second.trans_time = calculate_transmission_time(trans_speed, file_size);
    it->second.prop_time = calculate_propagation_time(prop_speed, it->second.dist);
    it->second.delay_time = it->second.trans_time + it->second.prop_time;

    it++;
  }
}

long double calculate_transmission_time(long double trans_speed, long long file_size) {
  return (file_size / 8) / trans_speed;
}

long double calculate_propagation_time(long double prop_speed, int dist) {
  return dist / prop_speed;
}

void print_calculations_result(Paths &paths) {
  cout << endl;
  cout << "The Server B has finished the calculation of the delays:" << endl;
  cout << "------------------------------------------" << endl;
  cout << "Destination                Delay" << endl;
  cout << "------------------------------------------" << endl;

  map<int, Node>::iterator it = paths.node_map.begin();
  while (it != paths.node_map.end()) {
    int id = it->first;
    long double delay = it->second.delay_time;

    cout << to_string(id) << "                         " << to_string_decimal_place(delay, 2) << endl;
    
    it++;
  }

  cout << "------------------------------------------" << endl;
}

string to_string_decimal_place(long double value, int decimal_place) {
  // modified version of this : https://stackoverflow.com/a/57459521/9560865
  double multiplier = pow(10.0, decimal_place);
  double rounded = round(value * multiplier) / multiplier;

  string val = to_string(rounded);
  int point_index = val.find_first_of(".");

  val = val.substr(0, point_index + decimal_place + 1);

  return val;
}

// payload format: node_id trans_time, prop_time delay_time (delimiter: -)
string create_response(Paths &paths) {
  map<int, Node>::iterator it = paths.node_map.begin();
  string response = "";

  while (it != paths.node_map.end()) {
    response  += to_string(it->second.id);
    response += " ";
    response += to_string(it->second.trans_time);
    response += " ";
    response += to_string(it->second.prop_time);
    response += " ";
    response += to_string(it->second.delay_time);
    response += "-";

    it++;
  }

  return response.substr(0, response.length() - 1);
}

void print_success_message() {
  cout << endl;
  cout << "The Server B has finished sending the output to AWS" << endl;
}