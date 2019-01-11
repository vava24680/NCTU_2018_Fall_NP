# -*- coding: utf-8 -*-
from base_server.base_server import BaseServer
from amqp_client_thread.amqp_client_thread import AMQPClientThread
import sys
import queue
from colored import fg, attr

class ApplicationServer(BaseServer):
  def __init__(self, IP: str, port: int):
    super().__init__(IP, port)
    print(self._server_config)
    self.__task_queue = queue.Queue()
    self.__amqp_client_thread = AMQPClientThread(
        self._server_config['RabbitMQ']['host'],
        self._server_config['RabbitMQ']['port'],
        self._server_config['RabbitMQ']['virtual_host'],
        self._server_config['RabbitMQ']['user'],
        self._server_config['RabbitMQ']['password'],
        self._server_config['RabbitMQ']['using_ssl'],
        self.__task_queue)
    self.__amqp_client_thread.start()
    self.__number_of_clients = 1

  def _add_new_group_record(self, group_name: str) -> None:
    self._mongo_database[self.GROUPS_COLLECTION].insert_one(
        {'group_name': group_name, 'public': True})

  def _add_new_joined_group_record(self, user_name: str, group_name: str)\
      -> None:
    self._mongo_database[self.JOINED_GROUPS_COLLECTION].insert_one(
        {'user_name': user_name, 'group_name': group_name})

  def _add_new_invitation_record(self, inviter: str, invitee: str) -> None:
    self._mongo_database[self.INVITATIONS_COLLECTION].insert_one(
        {'inviter': inviter, 'invitee': invitee})

  def _add_new_friendship(self, user_name_1: str, user_name_2: str) -> None:
    self._mongo_database[self.FRIENDSHIPS_COLLECTION].insert_one(
        {'user_name_1': user_name_1, 'user_name_2': user_name_2})

  def _add_new_post_record_by_user_name(self, user_name: str, message: str)\
      -> None:
    self._mongo_database[self.POSTS_COLLECTION].insert_one(
        {'user_name': user_name, 'message': message})

  def _get_all_public_groups_records(self):
    return self._mongo_database[self.GROUPS_COLLECTION].find({'public': True})

  def _get_invitations_records_by_invitee(self, user_name: str):
    return self._mongo_database[self.INVITATIONS_COLLECTION].find(
        {'invitee': user_name})

  def _get_friendships_records_by_user_name(self, user_name: str):
    return self._mongo_database[self.FRIENDSHIPS_COLLECTION].find(
        {'$or': [{'user_name_1': user_name}, {'user_name_2': user_name}]})

  def _get_posts_records_by_users_list(self, users_list: list):
    return self._mongo_database[self.POSTS_COLLECTION].find(
        {'user_name': {'$in': users_list}})

  def _check_login_by_user_name(self, user_name: str) -> bool:
    return self._mongo_database[self.LOGIN_USERS_COLLECTION].find_one(
        {'user_name': user_name}) is not None

  def _check_group_exists(self, group_name: str) -> bool:
    return self._mongo_database[self.GROUPS_COLLECTION].find_one(
        {'group_name': group_name}) is not None

  def _check_joined_group_record_exists(self, user_name: str, group_name: str)\
      -> bool:
    return self._mongo_database[self.JOINED_GROUPS_COLLECTION].find_one(
        {'user_name': user_name, 'group_name': group_name}) is not None

  def _check_invitation_record_exists(self, inviter: str, invitee: str) -> bool:
    return self._mongo_database[self.INVITATIONS_COLLECTION].find_one(
        {'inviter': inviter, 'invitee': invitee}) is not None

  def _check_friendship_record_exists(self, user_name_1: str, user_name_2: str)\
      -> bool:
    friendships_query_filter = {
        '$or': [
          {'user_name_1': user_name_1, 'user_name_2': user_name_2},
          {'user_name_1': user_name_2, 'user_name_2': user_name_1}
        ]}
    return self._mongo_database[self.FRIENDSHIPS_COLLECTION].find_one(
        friendships_query_filter) is not None

  def _delete_invitation_record_by_inviter_and_invitee(self, inviter: str,
                                                       invitee: str)\
      -> bool:
    return self._mongo_database[self.INVITATIONS_COLLECTION].delete_one(
        {'inviter': inviter, 'invitee': invitee}).deleted_count == 1

  def __list_invite(self, message_payload: str = None) -> None:
    print('In list-invite handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if message_payload is None or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: list-invite <user>')
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      self._response_dict['status'] = 0
      self._response_dict['invite'] = []
      invitations_query_result_cursor =\
          self._get_invitations_records_by_invitee(user_name)
      print(type(invitations_query_result_cursor))
      for invitation_document in invitations_query_result_cursor:
        self._response_dict['invite'].append(invitation_document['inviter'])

  def __list_friend(self, message_payload: str) -> None:
    print('In list-friend handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if message_payload is None or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: list-friend <user>')
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      self._response_dict['status'] = 0
      self._response_dict['friend'] = []
      friendships_query_result_cursor =\
          self._get_friendships_records_by_user_name(user_name)
      for friendship_document in friendships_query_result_cursor:
        if user_name != friendship_document['user_name_1']:
          self._response_dict['friend'].append(
              friendship_document['user_name_1'])
        else:
          self._response_dict['friend'].append(
              friendship_document['user_name_2'])

  def __receive_post(self, message_payload: str) -> None:
    print('In receive-post handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if message_payload is None or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: receive-post <user>')
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      self._response_dict['status'] = 0
      self._response_dict['post'] = []
      friends_list = []
      friendships_query_result_cursor =\
          self._get_friendships_records_by_user_name(user_name)
      for friendship_document in friendships_query_result_cursor:
        if user_name != friendship_document['user_name_1']:
          friends_list.append(friendship_document['user_name_1'])
        else:
          friends_list.append(friendship_document['user_name_2'])
      posts_query_result_cursor = self._get_posts_records_by_users_list(
          friends_list)
      for post_document in posts_query_result_cursor:
        self._response_dict['post'].append({'id': post_document['user_name'],
                                            'message': post_document['message']})

  def __invite(self, message_payload: str) -> None:
    print('In invite handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 2:
      self._no_login_handler()
      self._invalid_instruction_format_handler('Usage: invite <user> <id>')
    elif self._check_user_exists(user_name = payload_split_list[1]) is False:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} does not exist'.format(payload_split_list[1])
    elif self._check_invitation_record_exists(
        inviter = payload_split_list[1],
        invitee = login_user_document['user_name'])\
      is True:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} has invited you'.format(payload_split_list[1])
    elif self._check_friendship_record_exists(login_user_document['user_name'],
                                              payload_split_list[1])\
      is True:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} is already your friend'.format(payload_split_list[1])
    elif self._check_invitation_record_exists(
        inviter = login_user_document['user_name'],
        invitee = payload_split_list[1])\
      is True:
      self._token_validated_message_printer()
      self._response_dict['message'] = 'Already invited'
    elif login_user_document['user_name'] == payload_split_list[1]:
      self._token_validated_message_printer()
      self._response_dict['message'] = 'You cannot invite yourself'
    else:
      self._token_validated_message_printer()
      inviter_user_name = login_user_document['user_name']
      invitee_user_name = payload_split_list[1]
      self._add_new_invitation_record(inviter_user_name, invitee_user_name)

      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __accept_invite(self, message_payload: str) -> None:
    print('In accepe-invite handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 2:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler(
          'Usage: accept-invite <user> <id>')
    else:
      self._token_validated_message_printer()
      inviter_user_name = payload_split_list[1]
      invitee_user_name = login_user_document['user_name']

      if self._delete_invitation_record_by_inviter_and_invitee(
          inviter_user_name,
          invitee_user_name)\
        is False:
        self._response_dict['message'] =\
            '{} did not invite you'.format(inviter_user_name)
      else:
        self._add_new_friendship(inviter_user_name, invitee_user_name)
        self._response_dict['status'] = 0
        self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __post(self, message_payload: str) -> None:
    print('In post handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 2:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: post <user> <message>')
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      message = payload_split_list[1]
      print(message)
      self._add_new_post_record_by_user_name(user_name, message)
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE


  def __send(self, message_payload: str) -> None:
    print('In send handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 3:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler(
          'Usage: send <user> <friend> <message>')
    elif self._check_user_exists(user_name = payload_split_list[1]) is False:
      self._token_validated_message_printer()
      self._response_dict['message'] = 'No such user exist'
    elif self._check_friendship_record_exists(login_user_document['user_name'],
                                              payload_split_list[1])\
        is False:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} is not your friend'.format(
          payload_split_list[1])
    elif self._check_login_by_user_name(payload_split_list[1]) is False:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} is not online'.format(
          payload_split_list[1])
    else:
      self._token_validated_message_printer()
      sender = login_user_document['user_name']
      receiver = payload_split_list[1]
      message = payload_split_list[2]
      print(message)
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE
      self.__task_queue.put(
          {
            'task': 1,
            'data': {
              'sender': sender,
              'receiver': receiver,
              'message': message
            }
          })
      """
      self.__amqp_client_thread.publish_message_to_a_private_queue(sender, receiver,
                                                            message)
      """

  def __create_group(self, message_payload: str) -> None:
    print('In create-group handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 2:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: create-group <user> <group>')
    elif self._check_group_exists(group_name = payload_split_list[1]) is True:
      self._token_validated_message_printer()
      self._response_dict['message'] = '{} already exist'.format(payload_split_list[1])
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      group_name = payload_split_list[1]
      self._add_new_group_record(group_name)
      self._add_new_joined_group_record(user_name, group_name)
      self._response_dict['status'] = 0
      self._response_dict['topic'] = group_name
      self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __list_group(self, message_payload: str) -> None:
    print('In list-group handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: list-group <user>')
    else:
      self._token_validated_message_printer()
      self._response_dict['status'] = 0
      self._response_dict['all-groups'] = []
      all_groups_query_result_cursor = self._get_all_public_groups_records()

      for group_document in all_groups_query_result_cursor:
        self._response_dict['all-groups'].append(group_document['group_name'])

  def __list_joined(self, message_payload: str) -> None:
    print('In list-joined handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 1)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 1:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler('Usage: list-joined <user>')
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      self._response_dict['status'] = 0
      self._response_dict['all-joined-groups'] = []
      all_joined_groups_query_result_cursor =\
          self._get_all_joined_groups_of_a_user(user_name)

      for joined_group_document in all_joined_groups_query_result_cursor:
        self._response_dict['all-joined-groups'].append(
            joined_group_document['group_name'])

  def __join_group(self, message_payload: str) -> None:
    print('In join-group handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 2:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler(
          'Usage: join-group <user> <group>')
    elif self._check_group_exists(group_name = payload_split_list[1]) is False:
      self._token_validated_message_printer()
      self._response_dict['message'] =\
          '{} does not exist'.format(payload_split_list[1])
    elif self._check_joined_group_record_exists(
        user_name = login_user_document['user_name'],
        group_name = payload_split_list[1])\
        is True:
      self._token_validated_message_printer()
      self._response_dict['message'] =\
          'Already a member of {}'.format(payload_split_list[1])
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      group_name = payload_split_list[1]
      self._add_new_joined_group_record(user_name, group_name)

      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE
      self._response_dict['topic'] = group_name

  def __send_group(self, message_payload: str) -> None:
    print('In send-group handler')
    self._response_dict['status'] = 1
    if message_payload is not None:
      payload_split_list = message_payload.split(' ', 2)
      login_user_document = self._get_login_user_by_token(payload_split_list[0])
    else:
      payload_split_list = []
      login_user_document = None

    if not message_payload or login_user_document is None:
      self._no_login_handler()
    elif len(payload_split_list) != 3:
      self._token_validated_message_printer()
      self._invalid_instruction_format_handler(
          'Usage: send-group <user> <group> <message>')
    elif self._check_group_exists(group_name = payload_split_list[1]) is False:
      self._token_validated_message_printer()
      self._response_dict['message'] = 'No such group exist'
    elif self._check_joined_group_record_exists(
        user_name = login_user_document['user_name'],
        group_name = payload_split_list[1])\
        is False:
      self._token_validated_message_printer()
      self._response_dict['message'] =\
          'You are not the member of {}'.format(payload_split_list[1])
    else:
      self._token_validated_message_printer()
      user_name = login_user_document['user_name']
      group_name = payload_split_list[1]
      message = payload_split_list[2]
      self.__task_queue.put(
          {
            'task': 2,
            'data': {
              'sender': user_name,
              'exchange': group_name,
              'message': message
            }
          })
      """
      self.__amqp_client_thread.publish_message_to_a_fanout_exchange(
          sender = user_name,
          exchange = group_name,
          message = message)
      """
      self._response_dict['status'] = 0
      self._response_dict['message'] = self.SUCCESS_MESSAGE

  def __terminate_handler(self) -> None:
    self.__amqp_client_thread.terminate_amqp_connection()
    self.__task_queue.put({'task': 0})

  def _message_parsing(self, receive_message: str) -> None:
    command = None
    rest_payload = None
    print(
        '{}Original instruction: {}'.format(fg('light_cyan'), receive_message))
    split_list = receive_message.split(' ', 1)
    if len(split_list) > 1:
      command = split_list[0]
      rest_payload = split_list[1]
    if len(split_list) == 1:
      command = split_list[0]
    print('This is a {} request{}'.format(command, attr('reset')))

    {
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
      'terminate': lambda: self.__terminate_handler()
    }.get(command, lambda: self._unknown_command_handler(command))()

if __name__ == '__main__':
  if len(sys.argv) == 3:
    application_server_instance = ApplicationServer(sys.argv[1], int(sys.argv[2]))
    application_server_instance.run()
  else:
    print('Usage python3 {} IP port'.format(sys.argv[0]))
