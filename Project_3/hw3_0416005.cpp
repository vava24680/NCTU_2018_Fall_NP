#include "./server.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>

using namespace std;

inline void PrintUsageMessage(void) {
  cout << "\n";
  cout << "Usage: ./hw3_0416005.out IP_OPTION PORT_OPTION\n" << endl;
  cout << "IP_OPTION:" << endl;
  cout << "\t" << "-i, --ipv4 IP-Address\t"
      << "Specify the address that server binds to. "
      << "IP-Address must be IPv4 address" << endl;
  cout << "PORT_OPTION:" << endl;
  cout << "\t" << "-p, --port port\t\t"
      << "Specify the port that server binds to. "
      << "With non-root privilege, port must greater than 1024" << endl;
}

int ArgumentsParsing(int argc, char* argv[],
                      string& IP, int& port) {
  int option_character;
  int return_value = 0;
  struct option opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"ipv4", required_argument, NULL, 'i'},
    {"port", required_argument, NULL, 'p'}
  };
  while(-1 != (option_character = getopt_long(argc, argv, "i:p:h", opts, NULL)))
  {
    switch(option_character) {
      case 'i':
        IP.assign(optarg, strlen(optarg));
        break;
      case 'h':
        PrintUsageMessage();
        return_value = -1;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      default:
        cout << "Unknown option" << endl;
        break;
    }
  }
  return return_value;
}

int main(int argc, char* argv[]) {
  int port = 0;
  string IP(""), domain_name("");
  Server* server_instance = NULL;

  if (ArgumentsParsing(argc, argv, IP, port) != -1) {
    if (IP.empty() || !port) {
      PrintUsageMessage();
      exit(1);
    } else {
      cout << "IP:" << IP << endl;
      cout << "Port:" << port << endl;
      cout << "Mongo URL:" << MONGODB_URL << endl << endl;
      server_instance = new Server(IP, port);
      server_instance->Start();
    }
  }
  if (!server_instance) delete server_instance;
  return 0;
}
