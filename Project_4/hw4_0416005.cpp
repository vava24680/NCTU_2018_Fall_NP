#include "./server.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>

using namespace std;

inline void PrintUsageMessage(void) {
  cout << "\n";
  cout << "Usage: ./hw3_0416005.out IP_OPTION PORT_OPTION MQ_SERVER_OPTION\n\n";
  cout << "IP_OPTION:" << endl;
  cout << "\t" << "-I, --ipv4 IP-Address\t"
      << "Specify the address that server binds to. "
      << "IP-Address must be IPv4 address" << endl;
  cout << "PORT_OPTION:" << endl;
  cout << "\t" << "-P, --port port\t\t"
      << "Specify the port that server binds to. "
      << "With non-root privilege, port must greater than 1024" << endl;
  cout << "MQ SERVER OPTION:" << endl;
  cout << "\t" << "-i, --mq-ip\t"
      << "Specify the address that mq server binds to."
      << "IP-Address must be IPv4 address" << endl;
  cout << "\t" << "-p, --mq-port\t"
      << "Specify the port that mq server listens on." << endl;
}

int ArgumentsParsing(int argc, char* argv[],string& IP, unsigned int& port,
                     string& mq_ip, unsigned int& mq_port) {
  int option_character;
  int return_value = 0;
  struct option opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"ipv4", required_argument, NULL, 'I'},
    {"port", required_argument, NULL, 'P'},
    {"mq-ip", required_argument, NULL, 'i'},
    {"mq-port", required_argument, NULL, 'p'}
  };
  while(-1 != (option_character = getopt_long(argc, argv, "I:P:i:p:h", opts, NULL)))
  {
    switch(option_character) {
      case 'I':
        IP.assign(optarg, strlen(optarg));
        break;
      case 'h':
        PrintUsageMessage();
        return_value = -1;
        break;
      case 'P':
        port = atoi(optarg);
        break;
      case 'i':
        mq_ip.assign(optarg, strlen(optarg));
        break;
      case 'p':
        mq_port = atoi(optarg);
        break;
      default:
        cout << "Unknown option" << endl;
        break;
    }
  }
  return return_value;
}

int main(int argc, char* argv[]) {
  unsigned int port = 0, mq_port = 0;
  string IP(""), domain_name(""), mq_ip("");
  Server* server_instance = NULL;

  if (ArgumentsParsing(argc, argv, IP, port, mq_ip, mq_port) != -1) {
    if (IP.empty() || !port || mq_ip.empty() || !mq_port) {
      PrintUsageMessage();
      exit(1);
    } else {
      cout << "IP:" << IP << endl;
      cout << "Port:" << port << endl;
      cout << "MQ IP: " << mq_ip << endl;
      cout << "MQ port: " << mq_port << endl;
      cout << "Mongo URL:" << MONGODB_URL << endl << endl;
      server_instance = new Server(IP, port, mq_ip, mq_port);
      server_instance->Start();
    }
  }
  if (!server_instance) delete server_instance;
  return 0;
}
