# -*- coding: UTF-8 -*-
import threading
import json
import queue
import sys
import time
import pika

class AMQPClientThread(threading.Thread):
  PERSONAL_EXCHANGE = 'personal'
  def __init__(self, thread_id: int, client_name: str, data: dict,):
    threading.Thread.__init__(self)
    self.__thread_id            = thread_id
    self.__client_name          = client_name
    self.__exit_action          = ''
    self.__rabbitmq_server_host = data['rabbitmq_server_information']['host']
    self.__rabbitmq_amqp_port   = data['rabbitmq_server_information']['port']
    self.__rabbitmq_virtual_host = \
      data['rabbitmq_server_information']['virtual_host']
    self.__rabbitmq_user        = data['rabbitmq_server_information']['user']
    self.__rabbitmq_password    = data['rabbitmq_server_information']['password']
    self.__pika_credentials     = pika.PlainCredentials(
        self.__rabbitmq_user,
        self.__rabbitmq_password)
    self.__amqp_parameter       = pika.ConnectionParameters(
        self.__rabbitmq_server_host,
        self.__rabbitmq_amqp_port,
        self.__rabbitmq_virtual_host,
        self.__pika_credentials)
    self.__subscribed_queues_list   = data['initial_subscribed_queues'][:]
    self.__subscribed_topics_list   = data['initial_subscribed_topics'][:]
    self.__is_running_flag          = data['is_running_flag']
    self.__local_command_event      = data['local_command_event']
    self.__print_order_semaphore    = data['print_order_semaphore']
    self.__print_queue_message_done = data['print_queue_message_done']
    self.__print_topic_message_done = data['print_topic_message_done']
    self.__global_counter           = data['global_counter']

  def run(self) -> None:
    self.__amqp_connection = pika.BlockingConnection(self.__amqp_parameter)
    self.__amqp_channel = self.__amqp_connection.channel()

    self.__amqp_channel.exchange_declare(
        exchange = AMQPClientThread.PERSONAL_EXCHANGE, exchange_type = 'direct')
    for queue in self.__subscribed_queues_list:
      self.__amqp_channel.queue_declare(queue = '{}_personal'.format(queue))
      self.__amqp_channel.queue_bind('{}_personal'.format(queue),
                                     AMQPClientThread.PERSONAL_EXCHANGE,
                                     queue)
      self.__amqp_channel.basic_consume(
          consumer_callback = self.__message_handler,
          queue = '{}_personal'.format(queue))

    for topic in self.__subscribed_topics_list:
      self.__amqp_channel.exchange_declare(exchange = '{}_pub'.format(topic),
                                           exchange_type = 'fanout')
      queue_declare_result = self.__amqp_channel.queue_declare(exclusive = True)
      self.__amqp_channel.queue_bind(queue_declare_result.method.queue,
                                     '{}_pub'.format(topic))
      self.__amqp_channel.basic_consume(
          consumer_callback = self.__message_handler,
          queue = queue_declare_result.method.queue)

    self.__is_running_flag.set()
    self.__amqp_channel.start_consuming()

    unsubscribed_array_of_queues = \
        self.unsubscribe_array_of_queues(self.__subscribed_queues_list[:])
    if self.__exit_action == 'delete':
      self.delete_array_of_queues(unsubscribed_array_of_queues)
    self.__amqp_channel.close()
    self.__amqp_connection.close()
    #print('AMQP client thread terminates', file = sys.stderr)

  def __stop_callback(self) -> None:
    self.__amqp_channel.stop_consuming()

  def stop(self, action: str = None) -> None:
    self.__exit_action = action
    self.__amqp_connection.add_callback_threadsafe(self.__stop_callback)

  def __message_handler(
      self,
      channel: pika.adapters.blocking_connection.BlockingChannel,
      method: pika.spec.Basic.Deliver,
      properties: pika.spec.BasicProperties,
      body: bytes
  ) -> None:
    channel.basic_ack(delivery_tag = method.delivery_tag)
    if self.__local_command_event.is_set():
      self.__print_order_semaphore.acquire()
      self.__print_order_semaphore.release()
    try:
      message_object = json.loads(body.decode())
    except json.JSONDecodeError:
      print('Not json-formatted payload, stop parsing')
    else:
      if properties.type == 'private':
        print('<<<{}->{}: {}>>>'.format(message_object['sender'],
                                        message_object['receiver'],
                                        message_object['message']))
        if self.__local_command_event.is_set():
          self.__print_queue_message_done.set()
      elif properties.type == 'public':
        print('<<<{}->GROUP<{}>: {}>>>'.format(message_object['sender'],
                                               message_object['topic'],
                                               message_object['message']))
        if self.__local_command_event.is_set():
          number_finished_thread = self.__global_counter.get()
          number_finished_thread -= 1
          if number_finished_thread > 0:
            self.__global_counter.put(number_finished_thread)
          elif number_finished_thread == 0:
            self.__print_topic_message_done.set()
            self.__global_counter.put(0)
          else:
            self.__global_counter.put(0)
      else:
        print('Unrecognized destination type, can not parsed')

  def subscribe_a_queue(self, queue_name: str) -> None:
    self.__amqp_channel.queue_declare('{}_personal'.format(queue_name))
    self.__amqp_channel.queue_bind('{}_personal'.format(queue_name),
                                   AMQPClientThread.PERSONAL_EXCHANGE,
                                   queue_name)
    self.__amqp_channel.basic_consume(
        consumer_callback = self.__message_handler,
        queue = '{}_personal'.format(queue_name))
    self.__subscribed_queues_list.append(queue_name)

  def subscribe_array_of_queues(self, queues_list: list = []) -> None:
    for queue in queues_list:
      self.subscribe_a_queue(queue)

  def subscribe_a_topic(self, topic_name: str) -> None:
    self.__amqp_channel.exchange_declare(exchange = '{}_pub'.format(topic_name),
                                         exchange_type = 'fanout')
    queue_declare_result = self.__amqp_channel.queue_declare(exclusive = True)
    self.__amqp_channel.queue_bind(queue_declare_result.method.queue,
                                   '{}_pub'.format(topic_name))
    self.__amqp_channel.basic_consume(
        consumer_callback = self.__message_handler,
        queue = queue_declare_result.method.queue)
    self.__subscribed_topics_list.append(topic_name)

  def subscribe_array_of_topics(self,topics_list: list = []) -> None:
    for topic in topics_list:
      self.subscribe_a_topic(topic)

  def unsubscribe_a_private_queue(self, queue: str) -> str:
    self.__amqp_channel.queue_unbind('{}_personal'.format(queue),
                                     self.PERSONAL_EXCHANGE,
                                     queue)
    if queue in self.__subscribed_queues_list:
      self.__subscribed_queues_list.remove(queue)
      return queue
    else:
      return ''

  def unsubscribe_array_of_queues(self, queues_list: list = []) -> list:
    return_queues_list = []
    for queue in queues_list:
      return_queues_list.append(self.unsubscribe_a_private_queue(queue))
    return return_queues_list

  def delete_a_private_queue(self, queue: str) -> None:
    self.__amqp_channel.queue_delete('{}_personal'.format(queue))

  def delete_array_of_queues(self, queue_list: list = []) -> None:
    for queue in queue_list:
      self.delete_a_private_queue(queue)

  def unsubscribe_a_public_queue(self, queue: str) -> None:
    pass

  def __unsubscribe_array_of_topics(self, topics_list: list = []) -> None:
    pass
