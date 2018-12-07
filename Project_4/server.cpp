#include "server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bsoncxx/v_noabi/bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/v_noabi/bsoncxx/builder/stream/document.hpp"
#include "bsoncxx/v_noabi/bsoncxx/json.hpp"
#include "mongocxx/v_noabi/mongocxx/instance.hpp"
#include "mongocxx/v_noabi/mongocxx/client.hpp"
#include "mongocxx/v_noabi/mongocxx/uri.hpp"
#include "mongocxx/v_noabi/mongocxx/database.hpp"
#include "mongocxx/v_noabi/mongocxx/collection.hpp"
#include "mongocxx/v_noabi/mongocxx/cursor.hpp"
#include "mongocxx/v_noabi/mongocxx/stdx.hpp"
#include "mongocxx/v_noabi/mongocxx/exception/operation_exception.hpp"

#include "botan-2/botan/blake2b.h"
#include "botan-2/botan/hex.h"

#include "nlohmann/json.hpp"

#define USERS_COLLECTION "Users"
#define GROUPS_COLLECTION "Groups"
#define JOINED_GROUPS_COLLECTION "JoinedGroups"
#define LOGIN_USERS_COLLECTION "LoginUsers"
#define INVITATIONS_COLLECTION "Invitations"
#define FRIENDSHIPS_COLLECTION "Friendships"
#define POSTS_COLLECTION "Posts"

#define BUFFER_SIZE 6144
#define BLAKE2B_DIGEST_LENGTH 144
#define TOKEN_BYTE_LENGTH BLAKE2B_DIGEST_LENGTH / 8 * 2

#define REGISTER 0
#define LOGIN 1
#define DELETE 2
#define LOGOUT 3
#define LIST_INIVTE 4
#define LIST_FRIEND 5
#define RECEIVE_POST 6
#define INVITE 7
#define ACCEPT_INVITE 8
#define POST 9
#define SEND 10
#define CREATE_GROUP 11
#define LIST_GROUP 12
#define LIST_JOINED 13
#define JOIN_GROUP 14
#define SEND_GROUP 15

#define SUCCESS_MESSAGE "Success!"
#define NOT_LOGIN_MESSAGE "Not login yet"
#define ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE "\033[91m"
#define ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE "\033[92m"
#define ASCII_ESCAPE_CODE_BRIGHT_YELLOW_FG_CODE "\033[93m"
#define ASCII_ESCAPE_CODE_BRIGHT_BLUE_FG_CODE "\033[94m"
#define ASCII_ESCASE_CODE_BRIGHT_CYAN_FG_CODE "\033[96m"
#define ASCII_ESCAPE_CODE_RESET_CODE "\033[0m"

using namespace std;
using JSON = nlohmann::json;
using SUB_DOCUMENT = bsoncxx::builder::basic::sub_document;
using SUB_ARRAY = bsoncxx::builder::basic::sub_array;

void Server::Initial(void) {
  commands_index_book_.insert({
      {"register", REGISTER},
      {"login", LOGIN},
      {"delete", DELETE},
      {"logout", LOGOUT},
      {"list-invite", LIST_INIVTE},
      {"list-friend", LIST_FRIEND},
      {"receive-post", RECEIVE_POST},
      {"invite", INVITE},
      {"accept-invite", ACCEPT_INVITE},
      {"post", POST},
      {"send", SEND},
      {"create-group", CREATE_GROUP},
      {"list-group", LIST_GROUP},
      {"list-joined", LIST_JOINED},
      {"join-group", JOIN_GROUP},
      {"send-group", SEND_GROUP}
      });
  mongodb_database = mongodb_client[MONGODB_DATABASE];
  CreateCollection(USERS_COLLECTION);
  CreateCollection(GROUPS_COLLECTION);
  CreateCollection(JOINED_GROUPS_COLLECTION);
  CreateCollection(LOGIN_USERS_COLLECTION);
  CreateCollection(INVITATIONS_COLLECTION);
  CreateCollection(FRIENDSHIPS_COLLECTION);
  CreateCollection(POSTS_COLLECTION);
  users_collection_ = mongodb_database[USERS_COLLECTION];
  groups_collection_ = mongodb_database[GROUPS_COLLECTION];
  joined_groups_collection_ = mongodb_database[JOINED_GROUPS_COLLECTION];
  login_users_collection_ = mongodb_database[LOGIN_USERS_COLLECTION];
  invitations_collection_ = mongodb_database[INVITATIONS_COLLECTION];
  friendships_collection_ = mongodb_database[FRIENDSHIPS_COLLECTION];
  posts_collection_ = mongodb_database[POSTS_COLLECTION];

  amqp_client.Run();
}

void Server::SendResponse(int* client_socket_file_descriptor) {
  response_string_.assign(response_object.dump());
  unsigned int send_message_length = 0;
  unsigned int response_string_length = response_string_.length();
  cout << ASCII_ESCAPE_CODE_BRIGHT_YELLOW_FG_CODE
      << response_string_
      << ASCII_ESCAPE_CODE_RESET_CODE
      << endl;
  const char* response_string_in_c_format = response_string_.c_str();
  do {
    int temp = send(*client_socket_file_descriptor,
                    response_string_in_c_format + send_message_length,
                    response_string_length - send_message_length, 0);
    send_message_length += temp;
  } while(send_message_length < response_string_length);
  response_object.clear();
}

string Server::GenerateToken(const string& user_name_and_timestamp) {
  static Botan::Blake2b blake_hash(BLAKE2B_DIGEST_LENGTH);
  string token;
  blake_hash.update(user_name_and_timestamp);
  token = Botan::hex_encode(blake_hash.final());
  return token;
}

string Server::GetTimeStamp(const time_t& now) {
  string time_string("");
  char second[3], minute[3], hour[3], day[3];
  tm* time_struct = gmtime(&now);
  sprintf(second, "%d", time_struct->tm_sec);
  sprintf(minute, "%d", time_struct->tm_min);
  sprintf(hour, "%d", time_struct->tm_hour);
  sprintf(day, "%d", time_struct->tm_mday);
  time_string.assign(day);
  time_string.append("-");
  time_string.append(hour);
  time_string.append("-");
  time_string.append(minute);
  time_string.append("-");
  time_string.append(second);
  return time_string;
}

void Server::GetToken(const char* const instruction, string& token) {
  char* temp = new char[strlen(instruction)]{};
  memcpy(temp, instruction, strlen(instruction));
  strtok(temp, " ") ? token.assign(temp, strlen(temp)) : token.assign("");
  delete []temp;
}

template<class T>
bool Server::CheckTokenExists(T token) {
  auto login_users_query_filter = bsoncxx::builder::stream::document()
      << "token" << token
      << bsoncxx::builder::stream::finalize;
  login_user_view_ = login_users_collection_.find_one(
      login_users_query_filter.view());
  return login_user_view_ && !token.empty() ? true : false;
}

template<class T>
bool Server::CheckLoginByUsername(T user_name) {
  auto login_user_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto login_user_query_result = login_users_collection_.find_one(
      login_user_query_filter.view());
  return (login_user_query_result) ? true : false;
}

template <class T>
bool Server::CheckUserExists(T user_name) {
  auto user_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto user_query_result = users_collection_.find_one(user_query_filter.view());
  return (user_query_result) ? true : false;
}

template<class T>
bool Server::CheckGroupExists(T group_name) {
  auto group_query_filter = bsoncxx::builder::stream::document()
      << "group_name" << group_name
      << bsoncxx::builder::stream::finalize;
  auto group_query_result = groups_collection_.find_one(
      group_query_filter.view());
  return (group_query_result) ? true : false;
}

template <class T, class U>
bool Server::CheckJoinedGroupRelationshipExists(T group_name, U user_name) {
  auto joined_group_relationship_query_filter
      = bsoncxx::builder::stream::document()
      << "group_name" << group_name
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto joined_group_relationship_query_result = joined_groups_collection_
      .find_one(joined_group_relationship_query_filter.view());
  return (joined_group_relationship_query_result) ? true : false;
}

template <class T, class U>
bool Server::CheckInvitationExists(T inviter_user_name, U invitee_user_name) {
  auto invitation_query_filter = bsoncxx::builder::stream::document()
      << "inviter" << inviter_user_name
      << "invitee" << invitee_user_name
      << bsoncxx::builder::stream::finalize;
  auto invitation_query_result = invitations_collection_.find_one(
      invitation_query_filter.view());
  return invitation_query_result ? true : false;
}

template <class T, class U>
bool Server::CheckFriendshipExists(T user_name_1, U user_name_2) {
  auto friendship_query_filter = bsoncxx::builder::stream::document()
      << "$or" << bsoncxx::builder::stream::open_array
      << bsoncxx::builder::stream::open_document
      << "user_name_1" << user_name_1
      << "user_name_2" << user_name_2
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::open_document
      << "user_name_1" << user_name_2
      << "user_name_2" << user_name_1
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::close_array
      << bsoncxx::builder::stream::finalize;
  auto friendship_query_result = friendships_collection_.find_one(
      friendship_query_filter.view());
  return friendship_query_result ? true : false;
}

template <class T>
bool Server::ValidatePassword(const bsoncxx::document::view user_document_view,
                              T password) {
  string password__(password);
  return !password__.compare(
      user_document_view["password"].get_utf8().value.to_string());
}

void Server::NotLoginHandler(void) {
  cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << "! * token not found in database !" << endl
      << "! * user didn't login           !" << endl
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << ASCII_ESCAPE_CODE_RESET_CODE << endl;
  response_object["status"] = 1;
  response_object["message"] = "Not login yet";
}

void Server::AfterLoginValidatedHandler(void) const {
  cout << ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << "!   * token found in database   !" << endl
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << ASCII_ESCAPE_CODE_RESET_CODE << endl;
}

void Server::InvalidInstructionFormatMessagePrinter(void) const {
  cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << "! * Invalid instruction format! !" << endl
      << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl
      << ASCII_ESCAPE_CODE_RESET_CODE << endl;
}

bool Server::GetUsername(string& user_name) const {
  if (!login_user_view_) {
    return false;
  } else {
    user_name.assign(
        login_user_view_->view()["user_name"].get_utf8().value.to_string());
    return true;
  }
}

void Server::CreateCollection(const string& collection_name) {
  try {
    mongodb_database.create_collection(collection_name);
    cout << ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE;
    cout << "Create Collection :" << collection_name << " successfully" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  } catch (const mongocxx::operation_exception& exception) {
    cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE;
    cout << "Collection :" << collection_name << " has existed" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  }
}

template <class T>
bool Server::AddNewUser(T user_name, T password) {
  auto user_document = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << "password" << password
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = users_collection_.insert_one(
      user_document.view());
  return insert_one_result ? true : false;
}

template <class T>
bool Server::AddNewGroup(T group_name, bool is_public) {
  auto group_document = bsoncxx::builder::stream::document()
      << "group_name" << group_name
      << "public" << is_public
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = groups_collection_.insert_one(
      group_document.view());
  return insert_one_result ? true : false;
}

template <class T, class U>
bool Server::AddNewJoinedGroupRecord(T group_name, U user_name) {
  auto joined_group_document = bsoncxx::builder::stream::document()
      << "group_name" << group_name
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = joined_groups_collection_.insert_one(
      joined_group_document.view());
  return insert_one_result ? true : false;
}

template <class T, class U>
bool Server::AddNewLoginRecord(T user_name, U token) {
  auto login_user_document = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << "token" << token
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = login_users_collection_.insert_one(
      login_user_document.view());
  return insert_one_result ? true : false;
}

template <class T, class U>
bool Server::AddNewInvitation(T inviter_user_name, U invitee_user_name) {
  auto invitation_document_value = bsoncxx::builder::stream::document()
      << "inviter" << inviter_user_name
      << "invitee" << invitee_user_name
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = invitations_collection_.insert_one(
      invitation_document_value.view());
  return insert_one_result ? true : false;
}

template <class T, class U>
bool Server::AddNewFriendship(T user_name_1, U user_name_2) {
  auto friendship_document_value = bsoncxx::builder::stream::document()
      << "user_name_1" << user_name_1
      << "user_name_2" << user_name_2
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = friendships_collection_.insert_one(
      friendship_document_value.view());
  return insert_one_result ? true : false;
}

template <class T, class U>
bool Server::AddNewPost(T user_name, U message) {
  auto post_document_value = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << "message" << message
      << bsoncxx::builder::stream::finalize;
  auto insert_one_result = posts_collection_.insert_one(
      post_document_value.view());
  return insert_one_result ? true : false;
}

template<class T>
bsoncxx::stdx::optional<bsoncxx::document::value> Server::GetUser(
    T user_name) {
  auto user_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  return users_collection_.find_one(user_query_filter.view());
}

mongocxx::cursor Server::GetAllPublicGroups(void) {
  auto public_groups_query_filter = bsoncxx::builder::stream::document()
      << "public" << true
      << bsoncxx::builder::stream::finalize;
  return groups_collection_.find(public_groups_query_filter.view());
}

mongocxx::cursor Server::GetAllPrivateGroups(void) {
  auto private_groups_query_filter = bsoncxx::builder::stream::document()
      << "public" << false
      << bsoncxx::builder::stream::finalize;
  return groups_collection_.find(private_groups_query_filter.view());
}

template <class T>
mongocxx::cursor Server::GetAllJoinedGroupsOfUser(T user_name) {
  auto joined_groups_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  return joined_groups_collection_.find(joined_groups_query_filter.view());
}

template <class T>
bsoncxx::stdx::optional<bsoncxx::document::value> Server::GetLoginUserByUsername(
    T user_name) {
  auto login_user_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  return login_users_collection_.find_one(login_user_query_filter.view());
}

template <class T>
bsoncxx::stdx::optional<bsoncxx::document::value> Server::GetLoginUserByToken(
    T token) {
  auto login_user_query_filter = bsoncxx::builder::stream::document()
      << "token" << token
      << bsoncxx::builder::stream::finalize;
  return login_users_collection_.find_one(login_user_query_filter.view());
}

template <class T>
mongocxx::cursor Server::GetInvitationsByInviter(T inviter_user_name) {
  auto invitations_query_filter = bsoncxx::builder::stream::document()
      << "inviter" << inviter_user_name
      << bsoncxx::builder::stream::finalize;
  return invitations_collection_.find(invitations_query_filter.view());
}

template <class T>
mongocxx::cursor Server::GetInvitationsByInvitee(T invitee_user_name) {
  auto invitations_query_filter = bsoncxx::builder::stream::document()
      << "invitee" << invitee_user_name
      << bsoncxx::builder::stream::finalize;
  return invitations_collection_.find(invitations_query_filter.view());
}

template <class T>
mongocxx::cursor Server::GetAllFriendsofOneUser(T user_name) {
  auto friendships_query_filter = bsoncxx::builder::stream::document()
      << "$or" << bsoncxx::builder::stream::open_array
      << bsoncxx::builder::stream::open_document
      << "user_name_1" << user_name
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::open_document
      << "user_name_2" << user_name
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::close_array
      << bsoncxx::builder::stream::finalize;
  return friendships_collection_.find(friendships_query_filter.view());
}

mongocxx::cursor Server::GetAllPosts(const vector<string>& users) {
  auto posts_query_filter = bsoncxx::builder::basic::document();
  posts_query_filter.append(
      bsoncxx::builder::basic::kvp("user_name", [&users](SUB_DOCUMENT sub_doc) {
        sub_doc.append(
            bsoncxx::builder::basic::kvp("$in",[&users](SUB_ARRAY sub_array) {
              for(auto user : users) {
                sub_array.append(user);
              }
              }));
        }));
  return posts_collection_.find(posts_query_filter.view());
}

mongocxx::cursor Server::GetAllPosts(const string& user_name) {
  auto posts_query_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  return posts_collection_.find(posts_query_filter.view());
}

template <class T>
bool Server::DeleteUser(T user_name) {
  auto user_deletion_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = users_collection_.delete_one(
      user_deletion_filter.view());
  return deletion_result->deleted_count() ? true : false;
}

template <class T>
bool Server::DeleteOneGroup(T group_name) {
  auto group_deletion_filter = bsoncxx::builder::stream::document()
      << "group_name" << group_name
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = groups_collection_.delete_one(
      group_deletion_filter.view());
  return deletion_result->deleted_count() ? true : false;
}

template <class T>
bool Server::DeleteLoginRecordByUsername(T user_name) {
  auto login_user_deletion_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = login_users_collection_.delete_one(
      login_user_deletion_filter.view());
  return deletion_result->deleted_count() ? true : false;
}

template <class T>
bool Server::DeleteLoginRecordByToken(T token) {
  auto login_user_deletion_filter = bsoncxx::builder::stream::document()
      << "token" << token
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = login_users_collection_.delete_one(
      login_user_deletion_filter.view());
  return deletion_result->deleted_count() ? true : false;
}

template <class T, class U>
bool Server::DeleteOneInvitation(T inviter_user_name, U invitee_user_name) {
  auto invitation_deletion_filter = bsoncxx::builder::stream::document()
      << "inviter" << inviter_user_name
      << "invitee" << invitee_user_name
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = invitations_collection_.delete_one(
      invitation_deletion_filter.view());
  return deletion_result->deleted_count() ? true : false;
}

template <class T>
unsigned int Server::DeleteAllInvitations(T user_name) {
  auto invitations_deletion_filter = bsoncxx::builder::basic::document();
  invitations_deletion_filter.append(bsoncxx::builder::basic::kvp(
      "$or", [&user_name](SUB_ARRAY sub_array) -> SUB_ARRAY {
        sub_array.append([&user_name](SUB_DOCUMENT sub_doc) -> SUB_DOCUMENT {
          sub_doc.append(
              bsoncxx::builder::basic::kvp("inviter", user_name));
          return sub_doc;
        });
        sub_array.append([&user_name](SUB_DOCUMENT sub_doc) -> SUB_DOCUMENT {
          sub_doc.append(
              bsoncxx::builder::basic::kvp("invitee", user_name));
          return sub_doc;
        });
        return sub_array;
      }));
  auto deletion_result = invitations_collection_.delete_many(
      invitations_deletion_filter.view());
  return deletion_result->deleted_count();
}

template <class T>
unsigned int Server::DeleteAllFriends(T user_name) {
  auto friends_deletion_filter = bsoncxx::builder::stream::document()
      << "$or" << bsoncxx::builder::stream::open_array
      << bsoncxx::builder::stream::open_document
      << "user_name_1" << user_name
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::open_document
      << "user_name_2" << user_name
      << bsoncxx::builder::stream::close_document
      << bsoncxx::builder::stream::close_array
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = friendships_collection_.delete_many(
      friends_deletion_filter.view());
  return deletion_result->deleted_count();
}

template <class T>
unsigned int Server::DeleteAllPosts(T user_name) {
  auto posts_deletion_filter = bsoncxx::builder::stream::document()
      << "user_name" << user_name
      << bsoncxx::builder::stream::finalize;
  auto deletion_result = posts_collection_.delete_many(
      posts_deletion_filter.view());
  return deletion_result->deleted_count();
}

void Server::Register(char* instruction) {
  cout << "In register function" << endl;
  char* user_name = NULL;
  char* password = NULL;

  user_name = strtok(instruction, " ");
  password = strtok(NULL, " ");
  response_object["status"] = 1;

  if (NULL != strtok(NULL, " ") || !(user_name && password)) {
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: register <id> <password>";
  } else if (CheckUserExists<const char* const>(user_name)) {
    response_object["message"] = string(user_name) + " is already used";
  } else {
    AddNewUser<const char* const>(user_name, password);
    response_object["status"] = 0;
    response_object["message"] = SUCCESS_MESSAGE;
  }
}

void Server::Login(char* instruction, const time_t& now) {
  cout << "In login function" << endl;
  char* user_name = NULL;
  char* password = NULL;
  string token;
  bsoncxx::stdx::optional<bsoncxx::document::value> user_query_result;
  bsoncxx::stdx::optional<bsoncxx::document::value> login_user_query_result;

  user_name = strtok(instruction, " ");
  password = strtok(NULL, " ");
  response_object["status"] = 1;

  if (NULL != strtok(NULL, " ") || !(user_name && password)) {
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: login <id> <password>";
  } else if (!(user_query_result = GetUser<const char* const>(user_name))) {
    cout << "User not found" << endl;
    response_object["message"] = "No such user or password error";
  } else if (!ValidatePassword<const char* const>(
        user_query_result->view(), password)) {
    cout << "Password didn't match" << endl;
    response_object["message"] = "No such user or password error";
  } else {
    JSON queues_array = JSON::array({string(user_name)});
    JSON topics_array = JSON::array();
    auto joined_topics_query_result_cursor = GetAllJoinedGroupsOfUser<string>(
        string(user_name));
    response_object["status"] = 0;
    response_object["message"] = SUCCESS_MESSAGE;
    for(auto joined_topic_document_view : joined_topics_query_result_cursor) {
      topics_array.push_back(
          joined_topic_document_view["group_name"].get_utf8()
          .value.to_string());
    }
    response_object["queues_list"] = queues_array;
    response_object["topics_list"] = topics_array;
    if ((login_user_query_result = GetLoginUserByUsername<const char* const>(
        user_name))) {
      response_object["token"] = login_user_query_result->view()["token"]
          .get_utf8().value.to_string();
    } else {
      token.assign(GenerateToken(string(user_name) + GetTimeStamp(now)));
      AddNewLoginRecord<const char* const, const string&>(user_name, token);
      response_object["token"] = token;
    }
  }
}

void Server::Delete(char* instruction) {
  cout << "In delete function" << endl;
  string token("");
  string user_name;

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (NULL != strtok(NULL, " ")){
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: delete <user>";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    DeleteAllInvitations<const string&>(user_name);
    DeleteAllFriends<const string&>(user_name);
    DeleteAllPosts<const string&>(user_name);
    DeleteLoginRecordByToken<const string&>(token);
    DeleteUser<const string&>(user_name);
    response_object["status"] = 0;
    response_object["message"] = SUCCESS_MESSAGE;
  }
}

void Server::Logout(char* instruction) {
  cout << "In logout function" << endl;
  string token("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (NULL != strtok(NULL, " ")){
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: logout <user>";
  } else {
    AfterLoginValidatedHandler();
    DeleteLoginRecordByToken<const string&>(token);
    response_object["status"] = 0;
    response_object["message"] = "Bye!";
  }
}

void Server::ListInvite(char* instruction) {
  cout << "In ListInvite function" << endl;
  string token("");
  string user_name("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (NULL != strtok(NULL, " ")){
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: list-invite <user>";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    auto invitation_query_result_cursor =
        GetInvitationsByInvitee<const string&>(user_name);
    response_object["status"] = 0;
    JSON invitation_array = JSON::array();
    for(auto&& iterator : invitation_query_result_cursor) {
      invitation_array.push_back(
          iterator["inviter"].get_utf8().value.to_string());
    }
    response_object["invite"] = invitation_array;
  }
}

void Server::ListFriend(char* instruction) {
  cout << "In ListFriend function" << endl;
  string token("");
  string user_name("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (NULL != strtok(NULL, " ")) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: list-friend <user>";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    JSON friend_array = JSON::array();
    auto friendship_query_result_cursor =
        GetAllFriendsofOneUser<const string&>(user_name);
    response_object["status"] = 0;

    for(auto&& friendship_document_view : friendship_query_result_cursor) {
      string user_name_1(
          friendship_document_view["user_name_1"].get_utf8().value
          .to_string());
      string user_name_2(
          friendship_document_view["user_name_2"].get_utf8().value
          .to_string());
      if (user_name.compare(user_name_1)) {
        friend_array.push_back(user_name_1);
      } else {
        friend_array.push_back(user_name_2);
      }
    }
    response_object["friend"] = friend_array;
  }
}

void Server::ReceivePost(char* instruction) {
  cout << "In ReceivePost function" << endl;
  string token("");
  string user_name("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (NULL != strtok(NULL, " ")) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: receive-post <user>";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    JSON posts_array = JSON::array();
    JSON post_object = JSON::object();
    vector<string> friends_user_name_list;
    auto friendship_query_result_cursor =
        GetAllFriendsofOneUser<const string&>(user_name);
    response_object["status"] = 0;

    for(auto&& friendship_document_view : friendship_query_result_cursor) {
      string user_name_1(
          friendship_document_view["user_name_1"].get_utf8().value
          .to_string());
      string user_name_2(
          friendship_document_view["user_name_2"].get_utf8().value
          .to_string());
      if (user_name.compare(user_name_1)) {
        friends_user_name_list.push_back(user_name_1);
      } else {
        friends_user_name_list.push_back(user_name_2);
      }
    }

    auto posts_query_result_cursor = GetAllPosts(friends_user_name_list);

    for(auto&& post_document_view : posts_query_result_cursor) {
      post_object["id"] =
          post_document_view["user_name"].get_utf8().value.to_string();
      post_object["message"] =
          post_document_view["message"].get_utf8().value.to_string();
      posts_array.push_back(post_object);
    }
    response_object["post"] = posts_array;
  }
}

void Server::Invite(char* instruction) {
  cout << "In Invite function" << endl;
  string token("");
  string inviter_user_name("");
  string invitee_user_name("");
  auto document = bsoncxx::builder::stream::document();

  strtok(instruction, " ") ? token.assign(instruction, strlen(instruction)) : token;
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else {
    AfterLoginValidatedHandler();
    GetUsername(inviter_user_name);
    // inviter token validated, do the following things in order
    // 1. Check the format of instruction
    // 2. Check if the invitee exists
    // 3. Check if the invitee has invited you
    // 4. Check if invitee is alread inviter's friend
    // 5. Check if the invitation exists
    // 6. Check if the invitee is the inviter himself
    strtok(NULL, " ") ? 
        invitee_user_name.assign(
            instruction + token.length() + 1,
            strlen(instruction + token.length() + 1)) :
        invitee_user_name;
    cout << "invitee_user_name, " << invitee_user_name << endl;
    if (invitee_user_name.empty() || strtok(NULL, " ")) {
      InvalidInstructionFormatMessagePrinter();
      response_object["message"] = "Usage: invite <user> <id>";
    } else if (invitee_user_name.empty() ||
               !CheckUserExists<const string&>(invitee_user_name)) {
      response_object["message"] = invitee_user_name + " does not exist";
    } else if (CheckInvitationExists<const string&, const string&>(
          invitee_user_name, inviter_user_name)) {
      response_object["message"] = invitee_user_name + " has invited you";
    } else if (CheckFriendshipExists<const string&, const string&>(
          inviter_user_name, invitee_user_name)) {
      response_object["message"] = invitee_user_name + " is already your friend";
    } else if (CheckInvitationExists<const string&, const string&>(
          inviter_user_name, invitee_user_name)) {
      response_object["message"] = "Already invited";
    } else if (!inviter_user_name.compare(invitee_user_name)) {
      response_object["message"] = "You cannot invite yourself";
    } else {
      AddNewInvitation(inviter_user_name, invitee_user_name);
      response_object["status"] = 0;
      response_object["message"] = SUCCESS_MESSAGE;
    }
  }
}

void Server::AcceptInvite(char* instruction) {
  cout << "In AcceptInvite function" << endl;
  string token("");
  string inviter_user_name("");
  string invitee_user_name("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else {
    AfterLoginValidatedHandler();
    GetUsername(invitee_user_name);
    strtok(NULL, " ");
    inviter_user_name.assign(
        instruction + token.length() + 1,
        strlen(instruction + token.length() + 1));
    if (strtok(NULL, " ") || inviter_user_name.empty()) {
      InvalidInstructionFormatMessagePrinter();
      response_object["message"] = "Usage: accept-invite <user> <id>";
    } else if (!DeleteOneInvitation<const string&, const string&>(
          inviter_user_name, invitee_user_name)){
      response_object["message"] = inviter_user_name + " did not invite you";
    } else {
      AddNewFriendship<const string&, const string&>(inviter_user_name, 
                                                     invitee_user_name);
      response_object["status"] = 0;
      response_object["message"] = SUCCESS_MESSAGE;
    }
  }
}

void Server::Post(char* instruction) {
  cout << "In Post function" << endl;
  string token("");
  string message__;
  string user_name;
  unsigned int instruction_length = strlen(instruction);

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if (instruction_length != token.length()) {
    AfterLoginValidatedHandler();
    message__.assign(instruction + token.length() + 1,
                    strlen(instruction + token.length() + 1));
    cout << message__ << endl;
    GetUsername(user_name);
    AddNewPost<const string&, const string&>(user_name, message__);
    response_object["status"] = 0;
    response_object["message"] = SUCCESS_MESSAGE;
  } else {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: post <user> <message>";
  }
}

void Server::Send(char* instruction) {
  cout << "In Send function" << endl;
  string token("");
  string message__;
  string user_name;
  string friend_name;
  char* friend_name_in_c_string = NULL;
  char* message_in_c_string = NULL;

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  friend_name_in_c_string = strtok(NULL, " ");
  if (friend_name_in_c_string) {
    message_in_c_string = friend_name_in_c_string +
      strlen(friend_name_in_c_string) + 1;
  }
  response_object["status"] = 1;

  if (!CheckTokenExists(token)) {
    NotLoginHandler();
  } else if ((!friend_name_in_c_string) || (!message_in_c_string)) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: send <user> <friend> <message>";
  } else if (!CheckUserExists<string>(string(friend_name_in_c_string))) {
    AfterLoginValidatedHandler();
    response_object["message"] = "No such user exist";
  } else {
    GetUsername(user_name);
    if (!CheckFriendshipExists<string, string>(user_name,
        string(friend_name_in_c_string))) {
      AfterLoginValidatedHandler();
      response_object["message"] = string(friend_name_in_c_string)
          + " is not your friend";
    } else if(!CheckLoginByUsername<string>(string(friend_name_in_c_string))) {
      AfterLoginValidatedHandler();
      response_object["message"] = string(friend_name_in_c_string)
          + " is not online";
    } else {
      AfterLoginValidatedHandler();
      message__.assign(message_in_c_string, strlen(message_in_c_string));
      friend_name.assign(friend_name_in_c_string,
                         strlen(friend_name_in_c_string));
      cout << message__ << endl;
      response_object["status"] = 0;
      response_object["message"] = SUCCESS_MESSAGE;

      amqp_client.PublishMessageToOnePrivateQueue(friend_name, user_name,
                                                  message__);
    }
  }
}

void Server::CreateGroup(char* instruction) {
  cout << "In CreateGroup function" << endl;
  string token("");
  char* group_name_in_c_string = NULL;

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  group_name_in_c_string = strtok(NULL, " ");
  response_object["status"] = 1;

  if (!CheckTokenExists<string>(token)) {
    NotLoginHandler();
  } else if (!group_name_in_c_string || strtok(NULL, " ")) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: create-group <user> <group>";
  } else if (CheckGroupExists<string>(string(group_name_in_c_string))) {
    AfterLoginValidatedHandler();
    response_object["message"] = string(group_name_in_c_string)
        + " already exist";
  } else {
    AfterLoginValidatedHandler();
    AddNewGroup<string>(string(group_name_in_c_string), true);
    response_object["status"] = 0;
    response_object["message"] = SUCCESS_MESSAGE;
  }
}

void Server::ListGroup(char* instruction) {
  cout << "In ListGroup function" << endl;
  string token("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists<string>(token)) {
    NotLoginHandler();
  } else if (strtok(NULL, " ")) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: list-group <user>";
  } else {
    JSON public_groups_array = JSON::array();
    auto public_groups_query_result_cursor = GetAllPublicGroups();
    response_object["status"] = 0;
    for(auto&& public_group_document_view : public_groups_query_result_cursor) {
      public_groups_array.push_back(
          public_group_document_view["group_name"].get_utf8()
          .value.to_string());
    }
    response_object["all-groups"] = public_groups_array;
  }
}

void Server::ListJoined(char* instruction) {
  cout << "In ListJoined function" << endl;
  string token("");
  string user_name("");

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  response_object["status"] = 1;

  if (!CheckTokenExists<string>(token)) {
    NotLoginHandler();
  } else if (strtok(NULL, " ")) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: list-joined <user>";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    JSON joined_topics_array = JSON::array();
    auto joined_topics_query_result_cursor = GetAllJoinedGroupsOfUser<string>(
        user_name);
    response_object["status"] = 0;
    for(auto&& joined_topic_document_view : joined_topics_query_result_cursor) {
      joined_topics_array.push_back(
          joined_topic_document_view["group_name"].get_utf8()
          .value.to_string());
    }
    response_object["all-joined-topics"] = joined_topics_array;
  }
}

void Server::JoinGroup(char* instruction) {
  cout << "In JoinGroup function" << endl;
  string token("");
  string user_name("");
  char* group_name_in_c_string = NULL;

  strtok(instruction, " ");
  token.assign(instruction, strlen(instruction));
  group_name_in_c_string = strtok(NULL, " ");
  response_object["status"] = 1;

  if (!CheckTokenExists<string>(token)) {
    NotLoginHandler();
  } else if (strtok(NULL, " ") || !group_name_in_c_string) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: join-group <user> <group>";
  } else if (!CheckGroupExists<string>(string(group_name_in_c_string))) {
    AfterLoginValidatedHandler();
    response_object["message"] = string(group_name_in_c_string)
        + " does not exist";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    if (CheckJoinedGroupRelationshipExists<string, string>(
          string(group_name_in_c_string), user_name)) {
      response_object["message"] = "Already a member of "
          + string(group_name_in_c_string);
    } else {
      AddNewJoinedGroupRecord<string, string>(
          string(group_name_in_c_string), user_name);
      response_object["status"] = 0;
      response_object["message"] = SUCCESS_MESSAGE;
      response_object["topic"] = string(group_name_in_c_string);
    }
  }
}

void Server::SendGroup(char* instruction) {
  cout << "In SendGroup function" << endl;
  string token("");
  string user_name("");
  string message("");
  char* token_in_c_string = NULL;
  char* group_name_in_c_string = NULL;
  char* message_in_c_string = NULL;

  if ((token_in_c_string = strtok(instruction, " ")) != NULL)
    token.assign(token_in_c_string, strlen(token_in_c_string));

  group_name_in_c_string = strtok(NULL, " ");
  if (group_name_in_c_string)
    message_in_c_string = group_name_in_c_string + strlen(group_name_in_c_string) + 1;
  response_object["status"] = 1;

  if (!CheckTokenExists<string>(token)) {
    NotLoginHandler();
  } else if (!group_name_in_c_string || !message_in_c_string ||
      !strlen(message_in_c_string)) {
    AfterLoginValidatedHandler();
    InvalidInstructionFormatMessagePrinter();
    response_object["message"] = "Usage: send-group <user> <group> <message>";
  } else if (!CheckGroupExists<const char* const>(group_name_in_c_string)) {
    AfterLoginValidatedHandler();
    response_object["message"] = "No such group exist";
  } else {
    AfterLoginValidatedHandler();
    GetUsername(user_name);
    if (!CheckJoinedGroupRelationshipExists<const char* const, string>(
        group_name_in_c_string, user_name)) {
      response_object["message"] = string("You are not the member of ").append(
          group_name_in_c_string, strlen(group_name_in_c_string));
    } else {
      response_object["status"] = 0;
      response_object["message"] = SUCCESS_MESSAGE;
      message.assign(message_in_c_string, strlen(message_in_c_string));
      cout << "message:" << message << endl;
      amqp_client.PublishMessageToOnePublicQueue(
          string(group_name_in_c_string) + "_pub", user_name, message);
    }
  }
}

void Server::MessageParsing(char* instruction, const time_t& now) {
  char* command = NULL;
  cout << ASCII_ESCAPE_CODE_BRIGHT_BLUE_FG_CODE
      << "############### Begin of one request ###############"
      << ASCII_ESCAPE_CODE_RESET_CODE << endl;
  cout << ASCII_ESCASE_CODE_BRIGHT_CYAN_FG_CODE
      << "Original instruction: "
      << instruction << endl;
  command = strtok(instruction, " ");
  cout << "This is a " << command << " request"
      << ASCII_ESCAPE_CODE_RESET_CODE << endl;
  try {
    switch(commands_index_book_.at(string(command))) {
      case REGISTER:
        Register(instruction + strlen(command) + 1);
        break;
      case LOGIN:
        Login(instruction + strlen(command) + 1, now);
        break;
      case DELETE:
        Delete(instruction + strlen(command) + 1);
        break;
      case LOGOUT:
        Logout(instruction + strlen(command) + 1);
        break;
      case LIST_INIVTE:
        ListInvite(instruction + strlen(command) + 1);
        break;
      case LIST_FRIEND:
        ListFriend(instruction + strlen(command) + 1);
        break;
      case RECEIVE_POST:
        ReceivePost(instruction + strlen(command) + 1);
        break;
      case INVITE:
        Invite(instruction + strlen(command) + 1);
        break;
      case ACCEPT_INVITE:
        AcceptInvite(instruction + strlen(command) + 1);
        break;
      case POST:
        Post(instruction + strlen(command) + 1);
        break;
      case SEND:
        Send(instruction + strlen(command) + 1);
        break;
      case CREATE_GROUP:
        CreateGroup(instruction + strlen(command) + 1);
        break;
      case LIST_GROUP:
        ListGroup(instruction + strlen(command) + 1);
        break;
      case LIST_JOINED:
        ListJoined(instruction + strlen(command) + 1);
        break;
      case JOIN_GROUP:
        JoinGroup(instruction + strlen(command) + 1);
        break;
      case SEND_GROUP:
        SendGroup(instruction + strlen(command) + 1);
        break;
      default:
        cout << "Unrecognized command" << endl;
        response_object.clear();
        response_object["status"] = 1;
        response_object["message"] = "Unknown command " + string(command);
        break;
    }
  } catch (out_of_range& err) {
    cout << "Unrecognized command" << endl;
    response_object.clear();
    response_object["status"] = 1;
    response_object["message"] = "Unknown command " + string(command);
  }
}

Server::Server() : blake2b_hash_{BLAKE2B_DIGEST_LENGTH}, mongodb_instance{} {}
Server::Server(const char* IP__, const char* port__)
    : blake2b_hash_{BLAKE2B_DIGEST_LENGTH},
    mongodb_instance{},
    mongodb_client{mongocxx::uri{MONGODB_URL}}
  {
  // Covert doted IP address to binary in NBO format
  inet_pton(AF_INET, IP__, (void *)(&serverInfo_.sin_addr));
  // Covert port to binary in NBO format
  serverInfo_.sin_port = htons((uint16_t)atoi(port__));
  serverInfo_.sin_family = AF_INET;
  counter_ = 1;
  addr_length_ = sizeof(serverInfo_);
  receive_message_length_ = 0;
  master_socket_file_descriptor_ = 0;
  client_socket_file_descriptor_ = 0;
  receive_buffer_ = new char[BUFFER_SIZE]{};
  Initial();
}

Server::Server(const string& IP, const unsigned int& port, const string& mq_ip,
               const unsigned int& mq_port) :
    amqp_client{mq_ip, mq_port, "np_hw4_user", "np_hw4", "np_hw4"},
    blake2b_hash_{BLAKE2B_DIGEST_LENGTH},
    mongodb_instance{},
    mongodb_client{mongocxx::uri{MONGODB_URL}}
  {
  // Covert doted IP address to binary in NBO format
  inet_pton(AF_INET, IP.c_str(), (void *)(&serverInfo_.sin_addr));
  // Covert port to binary in NBO format
  serverInfo_.sin_port = htons((uint16_t)(port));
  serverInfo_.sin_family = AF_INET;
  counter_ = 1;
  addr_length_ = sizeof(serverInfo_);
  receive_message_length_ = 0;
  master_socket_file_descriptor_ = 0;
  client_socket_file_descriptor_ = 0;
  receive_buffer_ = new char[BUFFER_SIZE]{};
  Initial();
}

Server::~Server() {
  if (receive_buffer_ != NULL) delete[] receive_buffer_;
  cout << "Server shutdown" << endl;
}

void Server::Start() {
  master_socket_file_descriptor_ = socket(AF_INET, SOCK_STREAM, 0);

  if (bind(master_socket_file_descriptor_, (struct sockaddr*)&serverInfo_,
           addr_length_)) {
    char error_message[4096]{0};
    strerror_r(errno, error_message, 4096 - 1);
    perror(error_message);
    return;
  }
  listen(master_socket_file_descriptor_, 5);
  while(1) {
    client_socket_file_descriptor_ = accept(master_socket_file_descriptor_,
        (struct sockaddr*)&serverInfo_, &addr_length_);
    receive_message_length_ += recv(client_socket_file_descriptor_,
        receive_buffer_, BUFFER_SIZE, 0);
    now_ = time(NULL);
    cout << "No." << counter_ << " request" << endl;
    MessageParsing(receive_buffer_, now_);
    SendResponse(&client_socket_file_descriptor_);
    shutdown(client_socket_file_descriptor_, SHUT_RDWR);
    close(client_socket_file_descriptor_);
    memset((void*)receive_buffer_, 0, BUFFER_SIZE);
    ++counter_;
    cout << ASCII_ESCAPE_CODE_BRIGHT_BLUE_FG_CODE
        << "################ End of one request ################"
        << "\n\n"
        << ASCII_ESCAPE_CODE_RESET_CODE;
  }
  shutdown(master_socket_file_descriptor_, SHUT_RDWR);
  close(master_socket_file_descriptor_);
}
