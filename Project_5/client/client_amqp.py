import sys
import socket
import json
import os
import time
import logging
import yaml
from queue import LifoQueue, Queue
from threading import Thread, Event, Semaphore
from amqp_client.amqp_client_thread import AMQPClientThread


class Client(object):
  def __init__(self, ip: str, port: str, rabbitmq_server_information: dict):
    try:
      socket.inet_aton(ip)
      if 0 < int(port) < 65535 and \
          0 < rabbitmq_server_information['port'] < 65535:
        self.ip                            = ip
        self.port                          = int(port)
        self.__rabbitmq_server_information = rabbitmq_server_information
      else:
        raise Exception('Port value should between 1~65535')
      self.thread_id                       = 0
      self.cookie                          = {}
      self.current_user_name               = ""
      self.__users_joined_groups_dict      = {}
      self.__print_order_semaphore         = Semaphore(0)
      self.__local_command_event           = Event()
      self.__print_topic_message_done      = Event()
      self.__global_counter                = LifoQueue()
      self.__groups_and_its_quantity       = {}
      self.__amqp_clients_events_dict      = {}
      self.__amqp_clients_task_queues_dict = {}
      self.__amqp_clients_threads_dict     = {}

      self.__local_command_event.clear()
      self.__print_topic_message_done.clear()
    except Exception as e:
      print(e, file=sys.stderr)
      sys.exit(1)

  def run(self):
    while True:
      cmd = sys.stdin.readline().strip('\n')
      #if cmd == 'exit' + os.linesep:
      if cmd == 'exit':
        for client_name, thread in self.__amqp_clients_threads_dict.items():
          thread.stop()
          thread.join()
        return
      if cmd != os.linesep:
        try:
          with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            self.__local_command_event.set()
            # Choose server
            ip_port_pair = self.__choose_server(cmd)
            # print(ip_port_pair)
            s.connect(ip_port_pair)
            cmd = self.__attach_token(cmd)
            s.send(cmd.encode())
            resp = s.recv(4096).decode()
            self.__show_result(json.loads(resp), cmd)
            self.__local_command_event.clear()
        except Exception as e:
          print(e, file=sys.stderr)

  def __show_result(self, resp: dict, cmd: str = None):
    # print(resp)
    if 'message' in resp:
      print(resp['message'])

    if 'invite' in resp:
      if len(resp['invite']) > 0:
        for l in resp['invite']:
          print(l)
      else:
        print('No invitations')

    if 'friend' in resp:
      if len(resp['friend']) > 0:
        for l in resp['friend']:
          print(l)
      else:
        print('No friends')

    if 'post' in resp:
      if len(resp['post']) > 0:
        for p in resp['post']:
          print('{}: {}'.format(p['id'], p['message']))
      else:
        print('No posts')

    if 'all-groups' in resp:
      if len(resp['all-groups']):
        for group_name in resp['all-groups']:
          print(group_name)
      else:
        print('No groups')

    if 'all-joined-groups' in resp:
      if len(resp['all-joined-groups']):
        for group_name in resp['all-joined-groups']:
          print(group_name)
      else:
        print('No groups')

    if cmd:
      command = cmd.split()
      if resp['status'] == 0 and command[0] == 'login':
        self.cookie[command[1]] = {}
        self.cookie[command[1]]['token'] = resp['token']
        self.cookie[command[1]]['IP'] = resp['IP']
        self.cookie[command[1]]['port'] = resp['port']
        self.__add_new_amqp_client(command[1], resp['queues_list'][:],
                                   resp['topics_list'][:])
        self.__users_joined_groups_dict[command[1]] = []
        for topic in resp['topics_list']:
          if not topic in self.__groups_and_its_quantity:
            self.__groups_and_its_quantity[topic] = 0
          self.__groups_and_its_quantity[topic] += 1
          self.__users_joined_groups_dict[command[1]].append(topic)
        # print(self.cookie)

      if resp['status'] == 0 and command[0] == 'logout':
        # Call cleaner to notify the thread to terminate
        self.__logout_delete_cleaner()

      if resp['status'] == 0 and command[0] == 'delete':
        # Call cleaner with additional parameter to notify the thread to
        # terminate and delete all related private queues
        self.__logout_delete_cleaner('delete')

      if resp['status'] == 0 and command[0] == 'send':
        # Wait the thread finish print the message
        if command[2] in self.__amqp_clients_threads_dict:
          self.__print_order_semaphore.release()
          self.__amqp_clients_events_dict[command[2]].wait(3)
          self.__amqp_clients_events_dict[command[2]].clear()
          self.__print_order_semaphore.acquire()

      if resp['status'] == 0 and \
          (command[0] == 'join-group' or command[0] == 'create-group'):
        # Subscribe a new public topic
        self.__add_new_subscribed_topic_to_amqp_client_thread(
            self.current_user_name, [resp['topic']])
        # Update the related dict
        if not command[2] in self.__groups_and_its_quantity:
          self.__groups_and_its_quantity[resp['topic']] = 0
        self.__groups_and_its_quantity[resp['topic']] += 1
        # Update the user's joined topics record
        self.__users_joined_groups_dict[self.current_user_name].append(
            resp['topic'])

      if resp['status'] == 0 and command[0] == 'send-group':
        # Post the semaphore to make print order more reasonable
        self.__print_order_semaphore.release()
        # Put the number of users of this topic on queue for synchronization
        self.__global_counter.put(self.__groups_and_its_quantity[command[2]])

        # Wait all threads finish print the message
        self.__print_topic_message_done.wait(3)
        self.__print_topic_message_done.clear()
        # Clean the global counter queue
        try:
          self.__global_counter.get_nowait()
        except Empty:
          pass
        # Make the value of semaphore back to 0
        self.__print_order_semaphore.acquire(2)

  def __add_new_amqp_client(self, user_name: str, queues: list = [],
                            topics: list = []
  ) -> None:
    if not user_name in self.__amqp_clients_threads_dict:
      task_queue = LifoQueue()
      master_slave_event = Event()
      amqp_client_thread_running = Event()

      master_slave_event.clear()
      amqp_client_thread_running.clear()
      data = {
        'rabbitmq_server_information': self.__rabbitmq_server_information,
        'initial_subscribed_queues': queues,
        'initial_subscribed_topics': topics,
        'is_running_flag': amqp_client_thread_running,
        'task_queue': task_queue,
        'local_command_event': self.__local_command_event,
        'print_order_semaphore': self.__print_order_semaphore,
        'print_queue_message_done': master_slave_event,
        'print_topic_message_done': self.__print_topic_message_done,
        'global_counter': self.__global_counter
      }
      # New a instance of AMQP client thread
      thread = AMQPClientThread(self.thread_id, user_name, data)
      # Make the thread be active
      thread.start()

      self.__amqp_clients_events_dict[user_name] = master_slave_event
      self.__amqp_clients_task_queues_dict[user_name] = task_queue
      self.__amqp_clients_threads_dict[user_name] = thread

      # Wait until the thread has been active
      amqp_client_thread_running.wait()

  def __add_new_subscribed_topic_to_amqp_client_thread(self, user_name: str,
                                                       topics_list: list = []
  ) -> None:
    self.__amqp_clients_threads_dict[user_name].subscribe_array_of_topics(
        topics_list)

  def __attach_token(self, cmd: str = None):
    if cmd:
      command = cmd.split()
      if len(command) > 1:
        if command[0] != 'register' and command[0] != 'login':
          if command[1] in self.cookie:
            self.current_user_name = command[1]
            command[1] = self.cookie[command[1]]['token']
          else:
            command.pop(1)
      return ' '.join(command)
    else:
      return cmd

  def __logout_delete_cleaner(self, action: str = None) -> None:
    for topic in self.__users_joined_groups_dict[self.current_user_name]:
      if topic in self.__groups_and_its_quantity:
        self.__groups_and_its_quantity[topic] -= 1
        if self.__groups_and_its_quantity[topic] == 0:
          del self.__groups_and_its_quantity[topic]
    self.__amqp_clients_threads_dict[self.current_user_name].stop(action)
    self.__amqp_clients_threads_dict[self.current_user_name].join()
    del self.__users_joined_groups_dict[self.current_user_name]
    del self.__amqp_clients_task_queues_dict[self.current_user_name]
    del self.__amqp_clients_events_dict[self.current_user_name]
    del self.__amqp_clients_threads_dict[self.current_user_name]
    del self.cookie[self.current_user_name]

  def __choose_server(self, cmd):
    command_split_list = cmd.split(' ', 2)
    if command_split_list[0] == 'register' or command_split_list[0] == 'login'\
        or command_split_list[0] == 'logout'\
        or command_split_list[0] == 'delete':
      return (self.ip, self.port)
    elif len(command_split_list) > 1 and command_split_list[1] in self.cookie:
      return (self.cookie[command_split_list[1]]['IP'],
              self.cookie[command_split_list[1]]['port'])
    else:
      return (self.ip, self.port)

def launch_client(ip: str, port: str):
  with open('client_config.yml') as config_file:
    client_config = yaml.load(config_file)
  for key in client_config['RabbitMQ']:
    if client_config['RabbitMQ'][key] is None:
      print('Make sure each field in client_config.yml is filled in')
      return
  c = Client(ip, port, client_config['RabbitMQ'])
  c.run()

if __name__ == '__main__':
  if len(sys.argv) == 3:
    #logging.basicConfig(level=logging.DEBUG)
    launch_client(sys.argv[1], sys.argv[2])
  else:
    print('Usage: python3 {} IP PORT'.format(sys.argv[0]))
