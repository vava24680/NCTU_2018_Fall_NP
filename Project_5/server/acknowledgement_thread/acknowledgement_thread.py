# -*- coding: utf-8 -*-
import threading
import pika

class AcknowledgementThread(threading.Thread):
  def __init__(self, rabbit_mq_config: dict, connection_established_event: threading.Event):
    threading.Thread.__init__(self)
    self.__rabbit_mq_config = rabbit_mq_config
    self.__connection_established_event = connection_established_event
    self.__rabbit_mq_credentials = pika.PlainCredentials(
        rabbit_mq_config['user'], rabbit_mq_config['password'])
    self.__amqp_connection_params = pika.ConnectionParameters(
        rabbit_mq_config['host'],
        rabbit_mq_config['port'],
        rabbit_mq_config['virtual_host'],
        self.__rabbit_mq_credentials,
        ssl = rabbit_mq_config['using_ssl'])
  def run(self) -> None:
    self.amqp_connection = pika.BlockingConnection(
        self.__amqp_connection_params)
    self.amqp_channel = self.amqp_connection.channel()
    self.amqp_channel.exchange_declare(
        'AMQP_connection_established_ack_exchange')
    self.amqp_channel.queue_declare('login_server')
    self.amqp_channel.queue_bind('login_server',
                                 'AMQP_connection_established_ack_exchange',
                                 'login_server')
    self.amqp_channel.basic_consume(self.__message_handler, 'login_server')
    self.amqp_channel.start_consuming()
    self.amqp_channel.queue_unbind('login_server',
                                   'AMQP_connection_established_ack_exchange',
                                   'login_server')

  def stop(self) -> None:
    self.amqp_connection.add_callback_threadsafe(self.__stop_callback)

  def __stop_callback(self) -> None:
    self.amqp_channel.stop_consuming()

  def __message_handler(
      self,
      channel: pika.adapters.blocking_connection.BlockingChannel,
      method: pika.spec.Basic.Deliver,
      properties: pika.spec.BasicProperties,
      body: bytes
  ) -> None:
    channel.basic_ack(method.delivery_tag)
    self.__connection_established_event.set()
