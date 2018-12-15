#ifndef SERVER_AMQP_CLIENT_H
#define SERVER_AMQP_CLIENT_H
#include <string>

#include <amqpcpp.h>
#include "SimpleAmqpClient/SimpleAmqpClient.h"

using namespace std;

class ServerAMQPClient {
  public:
    ServerAMQPClient();
    ServerAMQPClient(string IP, unsigned int port, string username,
                        string passcode, string virtual_host);
    void Run();
    bool PublishMessageToOnePrivateQueue(const string& private_queue,
                                         const string& sender,
                                         const string& message);
    bool PublishMessageToOnePublicQueue(const string& public_queue,
                                        const string& sender,
                                        const string& message);

  private:
    static string ConstructMessagePayloadForPrivateQueue(
        const string& private_queue,
        const string& sender,
        const string& message);
    static string ConstructMessagePayloadForPublicQueue(
        const string& public_exchange,
        const string& sender,
        const string& message);

  private:
    string kPRIVATE_EXCHANGE_;
    string kPRIVATE_TYPE_;
    string kPUBLIC_TYPE_;
    string mq_server_ip_;
    unsigned int mq_server_amqp_port_;
    string username_;
    string passcode_;
    string virtual_host_;

  private:
    AmqpClient::Channel::ptr_t channel_;
};
#endif
