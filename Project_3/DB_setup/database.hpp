#include <iostream>
#include <cstring>
#include "mongocxx/v_noabi/mongocxx/instance.hpp"
#include "mongocxx/v_noabi/mongocxx/client.hpp"
#include "mongocxx/v_noabi/mongocxx/database.hpp"
#include "mongocxx/v_noabi/mongocxx/collection.hpp"

using namespace std;

class Database;

class Database {
  public:
    Database();
    void DropAllCollections(void);
    void DropCollection(const string& collection_name);
    void CreateAllCollections(void);
    void CreateCollection(const string& collection_name);
    void ListCollections(vector<string>& collection_names);
  private:
    mongocxx::instance mongodb_instance_ = mongocxx::instance();
    mongocxx::client mongodb_client_;
    mongocxx::database mongodb_database_;
};
