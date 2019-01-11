# -*- coding: UTF-8 -*-
import sys
import socket
import json
import yaml
import abc
from colored import fg, attr
from pymongo import MongoClient

class BaseServer(object, metaclass = abc.ABCMeta):
  def __init__(self, IP: str, port: int):
    with open('./config/server_config.yml') as config_file:
      self._server_config = yaml.load(config_file)
    for item in self._server_config:
      for key in self._server_config[item]:
        if self._server_config[item][key] is None:
          print('Make sure each field in server_config.yml is filled')
          sys.exit(1)
    if 65525 > port > 0 and \
        65535 > self._server_config['RabbitMQ']['port'] > 0:
      self._master_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      self._host = IP
      self._port = port
      self.__mongo_client = MongoClient(self._server_config['MONGO']['host'],
                                        self._server_config['MONGO']['port'],
                                        username =\
                                            self._server_config['MONGO']\
                                                ['user'],
                                        password =\
                                            self._server_config['MONGO']\
                                                ['password'],
                                        authSource =\
                                            self._server_config['MONGO']['database'],
                                        authMechanism = 'SCRAM-SHA-256')
      self._mongo_database =\
          self.__mongo_client[self._server_config['MONGO']['database']]
      self.USERS_COLLECTION = 'Users'
      self.LOGIN_USERS_COLLECTION = 'LoginUsers'
      self.GROUPS_COLLECTION = 'Groups'
      self.JOINED_GROUPS_COLLECTION = 'JoinedGroups'
      self.INVITATIONS_COLLECTION = 'Invitations'
      self.FRIENDSHIPS_COLLECTION = 'Friendships'
      self.POSTS_COLLECTION = 'Posts'
      self._response_dict = {}
    self._command_counter = 1
    self.SUCCESS_MESSAGE = 'Success!'

  def run(self) -> None:
    self._master_socket.bind((self._host, self._port))
    self._master_socket.listen(7)
    print('Server Start')
    while(1):
      slave_socket, remote_address = self._master_socket.accept()
      receive_message = slave_socket.recv(6144).decode('UTF-8')
      print(
          '{}No.{} request{}'.format(fg(14), self._command_counter,
                                     attr('reset')))
      print(
          '{}############### Begin of one request ###############{}'\
              .format(fg(12), attr('reset')))
      print(
          '{}Remote address: {}{}'.format(fg(14), remote_address,
                                          attr('reset')))
      self._message_parsing(receive_message)
      self._send_response(slave_socket)
      slave_socket.shutdown(socket.SHUT_RDWR)
      slave_socket.close()
      self._command_counter += 1
      print(
          '{}################ End of one request ################\n\n{}'\
              .format(fg(12), attr('reset')))
    self._master_socket.shutdown(socket.SHUT_RDWR)
    self._master_socket.close()

  def _send_response(self, slave_socket) -> None:
    total_sent_length = 0
    json_string = json.dumps(self._response_dict)
    message_length = len(json_string)
    print('{}{}{}'.format(fg(11), json_string, attr('reset')))
    while(total_sent_length < message_length):
      sent_length = slave_socket.send(
          json_string[total_sent_length:].encode('UTF-8'))
      if sent_length == 0:
        print('socket connection broken')
        break
      total_sent_length += sent_length
    self._response_dict.clear()

  def _get_login_user_by_token(self, token: str):
    return self._mongo_database[self.LOGIN_USERS_COLLECTION].find_one(
        {'token': token})

  def _get_all_joined_groups_of_a_user(self, user_name: str):
    return self._mongo_database[self.JOINED_GROUPS_COLLECTION].find(
        {'user_name': user_name}, {'_id': False, 'group_name': True})

  def _check_user_exists(self, user_name: str) -> None:
    return self._mongo_database[self.USERS_COLLECTION].find_one(
        {'user_name': user_name}) is not None

  def _no_login_handler(self) -> None:
    print('{}{}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!'.format(fg(9), attr('bold')))
    print('! * token not found in database !')
    print('! * user didn not login         !')
    print('!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!{}'.format(attr('reset')))
    self._response_dict['status'] = 1
    self._response_dict['message'] = 'Not login yet'

  def _unknown_command_handler(self, command: str) -> None:
    print('Unrecognized command')
    self._response_dict.clear()
    self._response_dict['status'] = 1;
    self._response_dict['message'] =\
        'Unknown command {}'.format(command)

  def _invalid_instruction_format_handler(self, usage_message: str) -> None:
    self._invalid_instruction_format_message_printer()
    self._response_dict['message'] = usage_message

  def _token_validated_message_printer(self) -> None:
    print('{}{}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!'.format(fg(10), attr('bold')))
    print('!   * token found in database   !')
    print('!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!{}'.format(attr('reset')))

  def _invalid_instruction_format_message_printer(self) -> None:
    print('{}{}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!'.format(fg(9), attr('bold')))
    print('! * Invalid instruction format! !')
    print('!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!{}'.format(attr('reset')))

  @abc.abstractmethod
  def _message_parsing(self, receive_message: str) -> None:
    pass
