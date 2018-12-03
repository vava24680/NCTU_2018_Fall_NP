#include <string>
#include <iostream>
#include <cstdio>
#include "server_stomp_client.hpp"

using namespace std;

ServerStompClient::ServerStompClient() {};
ServerStompClient::ServerStompClient(string IP, unsigned int port) {
  char port_in_c_str[10]{'\0'};
  mq_server_IP_ = IP;
  port_ = port;
  sprintf(port_in_c_str, "%u", port_);
  mq_server_connection_url = "tcp://" + IP + ":" + string(port_in_c_str)
      + "?wireFormat=" + PROTOCOL;
}

void ServerStompClient::Run() {
  // 0. Initialize the ActiveMQCPP library
  activemq::library::ActiveMQCPP::initializeLibrary();

  // 1. Create connection_factory
  connection_factory_.reset(
      new activemq::core::ActiveMQConnectionFactory(mq_server_connection_url));

  // 2. Create connection
  try {
    connection_.reset(connection_factory_->createConnection());
  } catch (const cms::CMSException cms_exception) {
    cms_exception.printStackTrace();
  }

  // 3. Create session
  try {
    session_.reset(connection_->createSession());
  } catch (const cms::CMSException cms_exception) {
    cms_exception.printStackTrace();
  }

  try {
    connection_->start();
    cout << "Server side stomp client is running" << endl;
  } catch (const cms::CMSException cms_exception) {
    cms_exception.printStackTrace();
  }
}

bool ServerStompClient::PublishMessageToOneQueue(string sender, string receiver,
                                                 string message) {
  // 1. Create a destination whose type is Queue
  unique_ptr<cms::Destination> queue_destination(
      session_->createQueue(receiver));

  // 2. Create a message producer
  unique_ptr<cms::MessageProducer> message_producer(
      session_->createProducer(queue_destination.get()));
  message_producer->setDeliveryMode(cms::DeliveryMode::PERSISTENT);

  // 3. Create message object from session instance
  unique_ptr<cms::TextMessage> text_message(session_->createTextMessage());

  // 4. Set the message object with the payload message
  text_message->setText(message);

  message_producer->send(text_message.get());
  return false;
}

bool ServerStompClient::PublishMessageToOneTopic(string sender, string topic,
                                                 string message) {
  // 1. Create a destuination object instance whose type is Topic
  unique_ptr<cms::Topic> topic_destination(
      session_->createTopic(string("/topic/").append(topic)));

  // 2. Create a message producer
  unique_ptr<cms::MessageProducer> message_producer(
      session_->createProducer(topic_destination.get()));
  message_producer->setDeliveryMode(cms::DeliveryMode::PERSISTENT);

  // 3. Create message object from session instance
  unique_ptr<cms::TextMessage> text_message(session_->createTextMessage());

  // 4. Set the message object with the payload message
  text_message->setText(message);

  message_producer->send(text_message.get());
  return false;
}
