import pika
import threading
import time

class AMQPClient(threading.Thread):
  def __init__(self):
    threading.Thread.__init__(self)
    self.__credential = pika.PlainCredentials('np_hw4_user', 'np_hw4')
    self.__parameter = pika.ConnectionParameters(host = 'npaws.haohao.in', virtual_host = 'np_hw4', credentials = self.__credential)
    self.__connection = pika.BlockingConnection(self.__parameter)
    self.__channel = self.__connection.channel()

  def run(self):
    print('AMQP client thread start')
    self.__channel.queue_declare(queue='helloG')
    self.__channel.basic_consume(self.__message_handler,
                                 queue='helloG',
                                 no_ack=True)
    self.__channel.start_consuming()
    print('AMQP client stop consuming')

  def __message_handler(self, channel, method, properties, body):
    print('channel: ', (channel))
    print('method: ', (method))
    print('properties: ', properties.type)
    print('[x] Received %r' % body)

  def stop(self):
    self.__channel.stop_consuming()

if __name__ == '__main__':
  print('Begin of the program')
  client = AMQPClient()
  client.start()
  time.sleep(8)
  client.stop()
  client.join()
  print('Exit of the program')
