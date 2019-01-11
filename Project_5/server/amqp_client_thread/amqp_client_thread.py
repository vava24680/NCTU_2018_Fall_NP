# -*- coding: utf-8 -*-
import pika
import json
import threading
import queue

class AMQPClientThread(threading.Thread):
  AMQP_CONNECTION_ESTABLISHED_ACK_EXCHANGE =\
      'AMQP_connection_established_ack_exchange'
  PERSONAL_EXCHANGE = 'personal'
  def __init__(self, ip: str, port: int, virtual_host: str, user: str,
               password: str,
               using_ssl: bool,
               task_queue: queue.Queue):
    threading.Thread.__init__(self)
    self.__host         = ip
    self.__port         = port
    self.__virtual_host = virtual_host
    self.__user         = user
    self.__password     = password
    self.__credentials  = pika.PlainCredentials(user, password)
    self.__amqp_connection_params = pika.ConnectionParameters(ip, port,
                                                              virtual_host,
                                                              self.__credentials,
                                                              ssl = using_ssl)
    self.__task_queue = task_queue
  def run(self) -> None:
    self.__amqp_connection = pika.BlockingConnection(
        self.__amqp_connection_params)
    self.__amqp_channel = self.__amqp_connection.channel()
    self.__amqp_channel.exchange_declare(exchange = AMQPClientThread.PERSONAL_EXCHANGE)
    self.__amqp_channel.basic_publish(
        AMQPClientThread.AMQP_CONNECTION_ESTABLISHED_ACK_EXCHANGE,
        'login_server',
        'established')
    print('AMQP connection established')
    while(True):
      try:
        task = self.__task_queue.get_nowait()
      except queue.Empty:
        self.__amqp_connection.process_data_events()
      else:
        if task['task'] == 1:
          self.__publish_message_to_a_private_queue(
              task['data'].get('sender', ''),
              task['data'].get('receiver', ''),
              task['data'].get('message', ''))
        elif task['task'] == 2:
          self.__publish_message_to_a_fanout_exchange(
              task['data'].get('sender', ''),
              task['data'].get('exchange', ''),
              task['data'].get('message', ''))
        else:
          pass

  def terminate_amqp_connection(self) -> None:
    self.__amqp_channel.close()
    self.__amqp_connection.close()

  def __publish_message_to_a_private_queue(self, sender: str, receiver: str,
                                         message: str) -> None:
    self.__amqp_channel.basic_publish('personal', receiver,
                                      json.dumps({'sender': sender,
                                                  'receiver': receiver,
                                                  'message': message}),
                                      pika.spec.BasicProperties(type = 'private'))

  def __publish_message_to_a_fanout_exchange(self, sender: str, exchange: str,
                                           message: str) -> None:
    self.__amqp_channel.basic_publish('{}_pub'.format(exchange), '',
        body = json.dumps({'sender': sender, 'topic': exchange,
                           'message': message}),
        properties = pika.spec.BasicProperties(type = 'public'))
