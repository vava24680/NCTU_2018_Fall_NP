#include "database.hpp"

#include <iostream>
#include <string>
#include <vector>

#include <mongocxx/v_noabi/mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/exception/exception.hpp>
#include <mongocxx/v_noabi/mongocxx/exception/operation_exception.hpp>
#include <mongocxx/v_noabi/mongocxx/database.hpp>
#include <mongocxx/v_noabi/mongocxx/collection.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>

#define MONGODB_HOST "localhost"
#define MONGODB_PORT "27018"
#define MONGODB_USER "np_user"
#define MONGODB_PASSWROD "2018NP"
#define MONGODB_DATABASE "NP_Project3"
#define MONGODB_LOGIN_AUTHENTICATION "SCRAM-SHA-256"
#define MONGODB_URL "mongodb://" MONGODB_USER  ":"  MONGODB_PASSWROD \
  "@" MONGODB_HOST ":" MONGODB_PORT "/?authSource=" MONGODB_DATABASE \
  "&authMechanism=" MONGODB_LOGIN_AUTHENTICATION

#define USERS_COLLECTION "Users"
#define LOGIN_USERS_COLLECTION "LoginUsers"
#define INVITATIONS_COLLECTION "Invitations"
#define FRIENDSHIPS_COLLECTION "Friendships"
#define POSTS_COLLECTION "Posts"

#define ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE "\033[91m"
#define ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE "\033[92m"
#define ASCII_ESCASE_CODE_BRIGHT_CYAN_FG_CODE "\033[96m"
#define ASCII_ESCAPE_CODE_RESET_CODE "\033[0m"

using namespace std;

Database::Database() {
  mongodb_client_ = mongocxx::client(mongocxx::uri(MONGODB_URL));
  mongodb_database_ = mongodb_client_[MONGODB_DATABASE];
}

void Database::DropAllCollections(void) {
  vector<string> collection_names(mongodb_database_.list_collection_names());
  for(auto collection_name : collection_names) {
    DropCollection(collection_name);
  }
}

void Database::DropCollection(const string& collection_name) {
  try {
    mongodb_database_[collection_name].drop();
    cout << ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE;
    cout << "Drop " << collection_name << " collection successfully" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  } catch (const mongocxx::operation_exception& exception) {
    cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE;
    cout << "Drop " << collection_name << " collection failed" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  }
}

void Database::CreateAllCollections(void) {
  static const vector<string> collection_names{USERS_COLLECTION,
      LOGIN_USERS_COLLECTION, INVITATIONS_COLLECTION,
      FRIENDSHIPS_COLLECTION, POSTS_COLLECTION};
  for(auto collection_name : collection_names) {
    CreateCollection(collection_name);
  }
}

void Database::CreateCollection(const string& collection_name) {
  try {
    mongodb_database_.create_collection(collection_name);
    cout << ASCII_ESCAPE_CODE_BRIGHT_GREEN_FG_CODE;
    cout << "Create " << collection_name << " collection successfully" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  } catch (const mongocxx::operation_exception& exception) {
    cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE;
    cout << "Create " << collection_name << " collection failed" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  }
}

void Database::ListCollections(vector<string>& collection_names) {
  try {
    collection_names = mongodb_database_.list_collection_names();
  } catch (const mongocxx::operation_exception& exception) {
    cout << ASCII_ESCAPE_CODE_BRIGHT_RED_FG_CODE;
    cout << "List collections failed" << endl;
    cout << ASCII_ESCAPE_CODE_RESET_CODE;
  }
}
