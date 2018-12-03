#ifndef SERVER_STOMP_CLIENT_H
#define SERVER_STOMP_CLIENT_H
#include <string>
#include <memory>

#include "activemq-cpp-3.9.4/activemq/core/ActiveMQConnectionFactory.h"
#include "activemq-cpp-3.9.4/activemq/util/Config.h"
#include "activemq-cpp-3.9.4/activemq/library/ActiveMQCPP.h"
#include "activemq-cpp-3.9.4/cms/Connection.h"
#include "activemq-cpp-3.9.4/cms/CMSException.h"
#include "activemq-cpp-3.9.4/cms/Session.h"
#include "activemq-cpp-3.9.4/cms/Destination.h"
#include "activemq-cpp-3.9.4/cms/Topic.h"
#include "activemq-cpp-3.9.4/cms/Queue.h"
#include "activemq-cpp-3.9.4/cms/MessageProducer.h"
#include "activemq-cpp-3.9.4/cms/Message.h"
#include "activemq-cpp-3.9.4/cms/TextMessage.h"
#include "activemq-cpp-3.9.4/cms/DeliveryMode.h"

using namespace std;
using namespace cms;
using namespace activemq;

#define PROTOCOL "stomp"

class ServerStompClient {
  public:
    ServerStompClient();
    ServerStompClient(string IP, unsigned int port);
    void Run();
    bool PublishMessageToOneTopic(string sender, string topic,
        string message);
    bool PublishMessageToOneQueue(string sender, string receiver,
        string message);
  public:
    static string ContructsMessagePayloadForQueue(const string& sender,
                                                  const string& receiver,
                                                  const string& message);
    static string ConstructMessagePayloadForTopic(const string& sender,
                                                  const string& topic,
                                                  const string& message);
  private:
    string mq_server_IP_;
    unsigned int port_;
    string mq_server_connection_url;
    unique_ptr<activemq::core::ActiveMQConnectionFactory> connection_factory_;
    unique_ptr<cms::Connection> connection_;
    unique_ptr<cms::Session> session_;
};
#endif
