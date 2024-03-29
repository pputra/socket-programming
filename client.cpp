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
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

#define AWS_PORT "24444" // the port client will be connecting to 
#define HOST_NAME "localhost"
#define MAXDATASIZE 100000 // max number of bytes we can get at once

void *get_in_addr(struct sockaddr*);
string parse_inputs(char*[]);
vector<string> split_string_by_delimiter(string, string);
void print_sent_message(string, string, string);
void print_calculation_result(string);
string to_string_decimal_place(long double, int);

int main(int argc, char *argv[]) {
  cout << "The client is up and running" << endl;
  int sockfd, numbytes;  
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc < 4) {
    fprintf(stderr,"usage: client hostname\n");
    exit(1);
  }

  string user_inputs = parse_inputs(argv);
  vector<string> inputs = split_string_by_delimiter(user_inputs, " ");
  
  string start_index = inputs[1];
  string map_id = inputs[0];
  string file_size = inputs[2];
 
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(HOST_NAME, AWS_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  
  if (send(sockfd, user_inputs.c_str(), user_inputs.length(), 0) == -1) {
     perror("send");
  }

  print_sent_message(start_index, map_id, file_size);

  char result[MAXDATASIZE];

  recv(sockfd, result, MAXDATASIZE - 1, 0);

  print_calculation_result(result);

  close(sockfd);

  return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

string parse_inputs(char *argv[]) {
   return string(argv[1]) + " " + 
    string(argv[2]) + " " + string(argv[3]);
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

void print_sent_message(string start_index, string map_id, string file_size) {
  cout << endl;
  cout << "The client has sent query to AWS using TCP: start vertex "
    << start_index << "; map "  << map_id << "; file size " << file_size << "." << endl;
}

void print_calculation_result(string result) {
  vector<string> split_result = split_string_by_delimiter(result, "-");

  cout << endl;
  cout << "The client has received results from AWS:" << endl;
  cout << "----------------------------------------------------------------------------" << endl;
  cout << "Destination    Min Length           Tt               Tp             Delay" << endl;
  cout << "----------------------------------------------------------------------------" << endl;

  for (int i = 0; i < split_result.size(); i++) {
    vector<string> node_data = split_string_by_delimiter(split_result[i], " ");
    int id = stoi(node_data[0]);
    int dist = stoi(node_data[1]);
    long double trans_time = stold(node_data[2]);
    long double prop_time = stold(node_data[3]);
    long double delay_time = stold(node_data[4]);

     cout << to_string(id) << "                " << to_string(dist) << "                  " 
      << to_string_decimal_place(trans_time, 2) 
      << "         " << to_string_decimal_place(prop_time, 2) << "          " 
      << to_string_decimal_place(delay_time, 2) << endl;
  }

  cout << "----------------------------------------------------------------------------" << endl;
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