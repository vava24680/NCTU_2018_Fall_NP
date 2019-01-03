#include <iostream>
#include <string>
#include <cstring>

#include "server_amqp_client.hpp"
#include "nlohmann/json.hpp"

using namespace std;
using JSON = nlohmann::json;

ServerAMQPClient::ServerAMQPClient() {};

ServerAMQPClient::ServerAMQPClient(string IP, unsigned int port,
                                         string username, string passcode,
                                         string virtual_host)
    : kPRIVATE_EXCHANGE_("personal"),
      kPRIVATE_TYPE_("private"),
      kPUBLIC_TYPE_("public") {
  mq_server_ip_ = IP;
  mq_server_amqp_port_ = port;
  username_ = username;
  passcode_ = passcode;
  virtual_host_ = virtual_host;
}


// Publish a message to a queue binded to personal exchange with a routing key
// The message is in json-string format which is composed by 3 key-value pairs:
// 1. (sender, sender username)
// 2. (receiver, private queue name)
// 3. (message, message body)
// @param private_queue name of private queue, which will be used as routing key
// @param username of sender
// @param message message body
bool ServerAMQPClient::PublishMessageToOnePrivateQueue(
    const string& private_queue,
    const string& sender,
    const string& message) {
  AmqpClient::BasicMessage::ptr_t amqp_message_payload 
      = AmqpClient::BasicMessage::Create(
          ServerAMQPClient::ConstructMessagePayloadForPrivateQueue(
              private_queue,
              sender,
              message));
  amqp_message_payload->Type(kPRIVATE_TYPE_);
  channel_->BasicPublish(kPRIVATE_EXCHANGE_, private_queue,
                         amqp_message_payload);
  return true;
}

bool ServerAMQPClient::PublishMessageToOnePublicQueue(
    const string& public_exchange,
    const string& sender,
    const string& message) {
  AmqpClient::BasicMessage::ptr_t amqp_message_payload =
    AmqpClient::BasicMessage::Create(
        ServerAMQPClient::ConstructMessagePayloadForPublicQueue(public_exchange,
                                                                sender,
                                                                message));
  amqp_message_payload->Type(kPUBLIC_TYPE_);
  channel_->DeclareExchange(public_exchange + "_pub",
                            AmqpClient::Channel::EXCHANGE_TYPE_FANOUT);
  channel_->BasicPublish(public_exchange + "_pub", "", amqp_message_payload);
  return true;
}

string ServerAMQPClient::ConstructMessagePayloadForPrivateQueue(
    const string& private_queue,
    const string& sender,
    const string& message) {
  JSON payload = JSON::object({
      {"sender", sender},
      {"receiver", private_queue},
      {"message", message}});
  return payload.dump();
}

string ServerAMQPClient::ConstructMessagePayloadForPublicQueue(
    const string& public_queue,
    const string& sender,
    const string& message) {
  JSON payload = JSON::object({
      {"sender", sender},
      {"topic", public_queue},
      {"message", message}});
  return payload.dump();
}

void ServerAMQPClient::Run() {
  channel_ = AmqpClient::Channel::Create(mq_server_ip_, mq_server_amqp_port_,
                                         username_, passcode_, virtual_host_);
  channel_->DeclareExchange(kPRIVATE_EXCHANGE_,
                            AmqpClient::Channel::EXCHANGE_TYPE_DIRECT);
}
