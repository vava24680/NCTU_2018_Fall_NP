# -*- coding: utf-8 -*-
from base_server.base_server import BaseServer
from acknowledgement_thread.acknowledgement_thread import AcknowledgementThread
import os
import sys
import hashlib
import yaml
import boto3
import socket
import time
import threading
from colored import fg, attr

class LoginServer(BaseServer):
  def __init__(self, IP: str, port: int):
    super().__init__(IP, port)
    with open('./config/aws_config.yml') as aws_config_file:
      self._aws_config = yaml.load(aws_config_file)
    for item in self._aws_config:
      for key in self._aws_config[item]:
        if self._aws_config[item][key] is None:
          print('Make sure each field in aws_config.yml is filled')
          sys.exit(1)
    self._aws_ec2_instance_config = self._aws_config['AWS_EC2']
    self._aws_ec2_resouce = boto3.resource('ec2',
                                           self._aws_config['AWS_region']\
                                               ['region'],
                                           aws_access_key_id =\
                                               self._aws_config\
                                                   ['AWS_credential']\
                                                   ['aws_access_key_id'],
                                           aws_secret_access_key =\
                                               self._aws_config\
                                                   ['AWS_credential']\
                                                   ['aws_secret_key'])
    print('IPv4 address of login server: ',
          self._aws_ec2_resouce.Instance('i-09bddf9b7576cc7ab')\
              .public_ip_address)
    self.__number_of_users = 0
    self.__application_ec2_instances_dict = {}
    self.__application_ec2_instances_list = []
    self.__instances_and_its_quantity_mapping_dict = {}
    self.__available_application_ec2_instances_stack = []
    self.__users_and_instances_mapping_dict = {}
    with open('./application_instance_startup_script.sh', 'r') as f:
      self.INIT_SCRIPTS = f.read()
    self.__amqp_connection_established_acknowledgement_event = threading.Event()
    self.__amqp_connection_established_acknowledgement_event.clear()
    self.__acknowledgement_thread = AcknowledgementThread(
        self._server_config['RabbitMQ'],
        self.__amqp_connection_established_acknowledgement_event)
    self.__acknowledgement_thread.start()
    self.__create_instances(1)
    print('{}{}{}'.format(fg('light_cyan'), 'Login Server Starts!!!!!', attr('reset')))

  def generate_token(self, user_name):
    sha256 = hashlib.sha256()
    sha256.update(user_name.encode('UTF-8'))
    sha256.update(os.urandom(8))
    return sha256.hexdigest()

  def cleaner(self):
    self.__acknowledgement_thread.stop()
    for instance_id in self.__available_application_ec2_instances_stack:
      if self.__instances_and_its_quantity_mapping_dict[instance_id] == 0:
        self.__application_ec2_instances_dict[instance_id].terminate()

  def _validate_password(self, user_name: str, password: str) -> bool:
    return self._mongo_database[self.USERS_COLLECTION].find_one(
        {'user_name': user_name, "password": password}) is not None

  def _add_new_user(self, user_name: str, password: str) -> None:
    self._mongo_database[self.USERS_COLLECTION].insert_one({
        'user_name': user_name, 'password': password})

  def _add_new_login_user_record(self, user_name: str, token: str) -> None:
    self._mongo_database[self.LOGIN_USERS_COLLECTION].insert_one(
        {'user_name': user_name, 'token': token})

  def _get_login_user_by_user_name(self, user_name: str):
    return self._mongo_database[self.LOGIN_USERS_COLLECTION].find_one(
        {'user_name': user_name})

  def _delete_user_record_by_user_name(self, user_name: str) -> bool:
    return self._mongo_database[self.USERS_COLLECTION].delete_one(
        {'user_name': user_name}).deleted_count == 1

  def _delete_login_record_by_token(self, token: str) -> bool:
    return self._mongo_database[self.LOGIN_USERS_COLLECTION].delete_one(
        {'token': token}).deleted_count == 1

  def _delete_joined_groups_records_by_user_name(self, user_name: str) -> int:
    return self._mongo_database[self.JOINED_GROUPS_COLLECTION].delete_many(
        {'user_name': user_name}).deleted_count

  def _delete_all_invitations_of_a_user(self, user_name: str) -> int:
    return self._mongo_database[self.INVITATIONS_COLLECTION].delete_many(
        {'$or': [{'inviter': user_name}, {'invitee': user_name}]}).deleted_count

  def _delete_friendships_records_by_user_name(self, user_name: str) -> int:
    return self._mongo_database[self.FRIENDSHIPS_COLLECTION].delete_many(
        {'$or': [{'user_name_1': user_name}, {'user_name_2': user_name}]})\
            .deleted_count

  def _delete_posts_records_by_user_name(self, user_name: str) -> int:
    return self._mongo_database[self.POSTS_COLLECTION].delete_many(
        {'user_name': user_name}).deleted_count

  # Create instances by using API provided by boto3
  def __create_instances(self, max_count_of_new_instances: int) -> int:
    instances_list = self._aws_ec2_resouce.create_instances(
        ImageId = self._aws_ec2_instance_config['image_id'],
        InstanceType = self._aws_ec2_instance_config['instance_type'],
        KeyName = self._aws_ec2_instance_config['key_name'],
        MaxCount = max_count_of_new_instances,
        MinCount = 1,
        Placement = {
            'AvailabilityZone':\
                self._aws_ec2_instance_config['availability_zone']
        },
        SecurityGroupIds = self._aws_ec2_instance_config['security_group_ids'],
        SubnetId = self._aws_ec2_instance_config['subnet_id'],
        UserData = self.INIT_SCRIPTS)
    for instance in instances_list:
      instance.wait_until_running()
      instance.reload()
      instance_id = instance.instance_id
      self.__application_ec2_instances_list.append(instance_id)
      self.__available_application_ec2_instances_stack.append(instance_id)
      self.__application_ec2_instances_dict[instance_id] = instance
      self.__instances_and_its_quantity_mapping_dict[instance_id]\
          = 0
    self.__amqp_connection_established_acknowledgement_event.wait()
    self.__amqp_connection_established_acknowledgement_event.clear()
    return len(instances_list)

  # Send terminate signal to application server to turn off the AMQP connection
  # on application server
  def __send_terminate_AMQP_connection_instruction_to_application_server(self,
      ip: str,
      port: int
  ) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect((ip, port))
      s.send('terminate'.encode())

  # Call by logout/delete handler after user successfully logout/deleted,
  # its goal is checking if the issued instance to the user is empty so
  # it can be terminated
  def __logout_delete_instance_handler(self, user_name: str) -> None:
    used_instance_id =\
        self.__users_and_instances_mapping_dict.pop(user_name, None)
    if used_instance_id is not None:
      self.__instances_and_its_quantity_mapping_dict[used_instance_id] -= 1

      print('all application_ec2_instances_list',
            self.__application_ec2_instances_list)
      print('available application_ec2_instances stack',
            self.__available_application_ec2_instances_stack)
      print('Instances and its quantity mapping',
            self.__instances_and_its_quantity_mapping_dict)

      # If the number of remaining clients on that application instance is 9,
      # put the id of this instacne back to available instance stack for new client
      if self.__instances_and_its_quantity_mapping_dict[used_instance_id] == 9:
        self.__available_application_ec2_instances_stack.append(used_instance_id)
      elif self.__instances_and_its_quantity_mapping_dict[used_instance_id] == 0:
        # Get the public IPv4 address of the instance to be terminated
        public_ipv4_address =\
            self.__application_ec2_instances_dict[used_instance_id]\
                .public_ip_address
        print(public_ipv4_address)
        # Ask application server to terminate the AMQP connection
        self.__send_terminate_AMQP_connection_instruction_to_application_server(
            public_ipv4_address,
            10778)
        time.sleep(0.1)
        self.__application_ec2_instances_dict[used_instance_id].terminate()
        self.__available_application_ec2_instances_stack.remove(used_instance_id)
        self.__application_ec2_instances_list.remove(used_instance_id)
        del self.__instances_and_its_quantity_mapping_dict[used_instance_id]
        del self.__application_ec2_instances_dict[used_instance_id]
      else:
        pass

      print('all application_ec2_instances_list',
            self.__application_ec2_instances_list)
      print('available application_ec2_instances stack',
            self.__available_application_ec2_instances_stack)
      print('Instances and its quantity mapping',
            self.__instances_and_its_quantity_mapping_dict)

  def __register(self, message_payload: str) -> None:
    print('In register handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ')
    else:
      payload_split_list = []

    if not message_payload or len(payload_split_list) != 2:
      self._invalid_instruction_format_handler(
          'Usage: register <id> <password>')
    elif self._check_user_exists(user_name = payload_split_list[0]) is True:
      self._response_dict['message'] =\
          '{} is already used'.format(payload_split_list[0])
    else:
      user_name, password = payload_split_list[0], payload_split_list[1]
      self._add_new_user(user_name, password)
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __login(self, message_payload: str) -> None:
    print('In login handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ')

    if message_payload is None or len(payload_split_list) != 2:
      self._invalid_instruction_format_handler('Usage: login <id> <password>')
    elif self._check_user_exists(user_name = payload_split_list[0]) is False:
      print(
          '{}!!!! user: {} not found{}'.format(fg('9'), payload_split_list[0],
                                               attr('reset')))
      self._response_dict['message'] = 'No such user or password error'
    elif self._validate_password(user_name = payload_split_list[0],
                                 password = payload_split_list[1]) is False:
      print(
          '{}!!!! user: {} password did not match'.format(fg('9'),
                                                          payload_split_list[0],
                                                          attr('reset')))
      self._response_dict['message'] = 'No such user or password error'
    else:
      user_name, password = payload_split_list[0], payload_split_list[1]
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE
      self._response_dict['queues_list'] = [user_name]
      self._response_dict['topics_list'] = []
      joined_groups = self._get_all_joined_groups_of_a_user(user_name)
      login_user_document = self._get_login_user_by_user_name(user_name)

      # Put all joined groups' names in the response object
      for document in joined_groups:
        self._response_dict['topics_list'].append(document['group_name'])

      # Check if the user has logged in
      if login_user_document is not None:
        print('User {} has logged in!!!'.format(user_name))
        self._response_dict['token'] = login_user_document['token']
        id_of_available_instance =\
            self.__users_and_instances_mapping_dict[user_name]
        # Since the user has logged in, so there's a record for its assigned
        # application server
        self._response_dict['IP'] =\
            self.__application_ec2_instances_dict[id_of_available_instance]\
                .public_ip_address
        self._response_dict['port'] = 10778
      else:
        # First time the user log in
        print('User {} first login'.format(user_name))
        # Generate a token
        token = self.generate_token(user_name)
        # Add new log-in record in the database
        self._add_new_login_user_record(user_name, token)

        # Increase the number of users by 1
        self.__number_of_users += 1

        # Determine whether there is no available application ec2 instance
        if len(self.__available_application_ec2_instances_stack) == 0:
          print('Create new application instance')
          # Create new instances which will run application server
          self.__create_instances(1)

        # Get id of one available instance
        id_of_available_instance =\
            self.__available_application_ec2_instances_stack.pop()
        # Get its public ipv4 address
        public_ipv4_address =\
            self.__application_ec2_instances_dict[id_of_available_instance]\
                .public_ip_address
        # Add user and its assigned instance id mapping in the database
        self.__users_and_instances_mapping_dict[user_name] = id_of_available_instance
        # Increase the user quantity of this available instance
        self.__instances_and_its_quantity_mapping_dict[id_of_available_instance] += 1
        # If the user quantity of this available instance hasn't exceeded 10,
        # put it back to the available instance stack
        if self.__instances_and_its_quantity_mapping_dict[id_of_available_instance] < 10:
          self.__available_application_ec2_instances_stack.append(id_of_available_instance)

        self._response_dict['token'] = token
        self._response_dict['IP'] = public_ipv4_address
        self._response_dict['port'] = 10778

      print('all application_ec2_instances_list',
            self.__application_ec2_instances_list)
      print('available application_ec2_instances stack',
            self.__available_application_ec2_instances_stack)
      print('Instances_and_its_quantity_ mapping_dict',
            self.__instances_and_its_quantity_mapping_dict)

  def __delete(self, message_payload: str) -> None:
    print('In delete handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])

    if message_payload is None or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._invalid_instruction_format_handler('Usage: delete <user>')
    else:
      user_name = login_user_document['user_name']
      self._token_validated_message_printer()
      self._delete_joined_groups_records_by_user_name(user_name)
      self._delete_all_invitations_of_a_user(user_name)
      self._delete_friendships_records_by_user_name(user_name)
      self._delete_posts_records_by_user_name(user_name)
      self._delete_login_record_by_token(login_user_document['token'])
      self._delete_user_record_by_user_name(user_name)
      self.__logout_delete_instance_handler(user_name)
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __logout(self, message_payload: str) -> None:
    print('In logout handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])

    if message_payload is None or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._invalid_instruction_format_handler('Usage: logout <user>')
    else:
      user_name = login_user_document['user_name']
      self._token_validated_message_printer()
      self._delete_login_record_by_token(login_user_document['token'])
      self.__logout_delete_instance_handler(user_name)
      self._response_dict['status'] = 0
      self._response_dict['message'] = 'Bye!'

  def __list_invite(self, message_payload: str) -> None:
    print('In list-invite handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __list_friend(self, message_payload: str) -> None:
    print('In list-friend handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __receive_post(self, message_payload: str) -> None:
    print('In receive-post handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __invite(self, message_payload: str) -> None:
    print('In invite handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __accept_invite(self, message_payload: str) -> None:
    print('In accept-invite handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __post(self, message_payload: str) -> None:
    print('In post handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __send(self, message_payload: str) -> None:
    print('In send handler')
    self._response_dict['status'] = 1
    self._no_login_handler()
    pass

  def __create_group(self, message_payload: str) -> None:
    print('In create-group handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __list_group(self, message_payload: str) -> None:
    print('In list-group handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __list_joined(self, message_payload: str) -> None:
    print('In list-joined handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __join_group(self, message_payload: str) -> None:
    print('In join-group handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def __send_group(self, message_payload: str) -> None:
    print('In send_group handler')
    self._response_dict['status'] = 1
    self._no_login_handler()

  def _message_parsing(self, receive_message):
    command = None
    rest_payload = None
    print(
        '{}Original instruction: {}'.format(fg('light_cyan'), receive_message))
    split_list = receive_message.split(' ', 1)
    if len(split_list) > 1:
      rest_payload = split_list[1]
      command = split_list[0]
    if len(split_list) == 1:
      command = split_list[0]
    print('This is a {} request {}'.format(command, attr('reset')))

    {
      'register': lambda: self.__register(rest_payload),
      'login': lambda: self.__login(rest_payload),
      'delete': lambda: self.__delete(rest_payload),
      'logout': lambda: self.__logout(rest_payload),
      'list-invite': lambda: self.__list_invite(rest_payload),
      'list-friend': lambda: self.__list_friend(rest_payload),
      'receive-post': lambda: self.__receive_post(rest_payload),
      'invite': lambda: self.__invite(rest_payload),
      'accept-invite': lambda: self.__accept_invite(rest_payload),
      'post': lambda: self.__post(rest_payload),
      'send': lambda: self.__send(rest_payload),
      'create-group': lambda: self.__create_group(rest_payload),
      'list-group': lambda: self.__list_group(rest_payload),
      'list-joined': lambda: self.__list_joined(rest_payload),
      'join-group': lambda: self.__join_group(rest_payload),
      'send-group': lambda: self.__send_group(rest_payload),
    }.get(command, lambda: self._unknown_command_handler(command))()


if __name__ == '__main__':
  if len(sys.argv) == 3:
    login_server_instance = LoginServer(sys.argv[1], int(sys.argv[2]))
    try:
      login_server_instance.run()
    except KeyboardInterrupt:
      login_server_instance.cleaner()
      print(
          '\n{}{}{}'.format(fg('light_red'), 'Login Server terminated',
                            attr('reset')))
      sys.exit(0)
  else:
    print('Usage: python3 {} IP port'.format(sys.argv[0]))
