#ifndef SERVER_H
#define SERVER_H
#include <string>
#include <cstring>
#include <unordered_map>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bsoncxx/v_noabi/bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/v_noabi/bsoncxx/builder/stream/document.hpp"
#include "bsoncxx/v_noabi/bsoncxx/json.hpp"
#include "mongocxx/v_noabi/mongocxx/instance.hpp"
#include "mongocxx/v_noabi/mongocxx/client.hpp"
#include "mongocxx/v_noabi/mongocxx/database.hpp"
#include "mongocxx/v_noabi/mongocxx/collection.hpp"
#include "mongocxx/v_noabi/mongocxx/stdx.hpp"

#include "botan-2/botan/blake2b.h"
#include "botan-2/botan/hex.h"

#include "nlohmann/json.hpp"

#define MONGODB_HOST "localhost"
#define MONGODB_PORT "27018"
#define MONGODB_USER "np_user"
#define MONGODB_PASSWROD "2018NP"
#define MONGODB_DATABASE "NP_Project3"
#define MONGODB_LOGIN_AUTHENTICATION "SCRAM-SHA-256"
#define MONGODB_URL "mongodb://" MONGODB_USER  ":"  MONGODB_PASSWROD \
  "@" MONGODB_HOST ":" MONGODB_PORT "/?authSource=" MONGODB_DATABASE \
  "&authMechanism=" MONGODB_LOGIN_AUTHENTICATION




using namespace std;
using JSON = nlohmann::json;

class Server;

class Server {
  public:
    Server();
    Server(const char* IP, const char* port);
    Server(const string& IP, const int port);
    void Start();
    ~Server();
  private:
    char* receive_buffer_;
    uint64_t counter_;
    unsigned int addr_length_;
    unsigned int receive_message_length_;
    int master_socket_file_descriptor_;
    int client_socket_file_descriptor_;
    string response_string_;
    JSON response_object;
    struct sockaddr_in serverInfo_;
    time_t now_;
    unordered_map<string, unsigned int> commands_index_book_;
  private:
    Botan::Blake2b blake2b_hash_;
    mongocxx::instance mongodb_instance;
    mongocxx::client mongodb_client;
    mongocxx::database mongodb_database;
    mongocxx::collection users_collection_;
    mongocxx::collection groups_collection_;
    mongocxx::collection joined_groups_collection_;
    mongocxx::collection login_users_collection_;
    mongocxx::collection invitations_collection_;
    mongocxx::collection friendships_collection_;
    mongocxx::collection posts_collection_;
    bsoncxx::stdx::optional<bsoncxx::document::value> login_user_view_;
  private:
    void Initial(void);
    void SendResponse(int* client_socket_file_descriptor);
    string GenerateToken(const string& user_name_and_timestamp);
    string GetTimeStamp(const time_t& now);
    void GetToken(const char* const instruction, string& token);
    template<class T>
      bool CheckTokenExists(T token);
    template<class T>
      bool CheckLoginByUsername(T user_name);
    template <class T>
      bool CheckUserExists(T user_name);
    template <class T>
      bool CheckGroupExists(T group_name);
    template <class T, class U>
      bool CheckJoinedGroupRelationshipExists(T group_name, U user_name);
    template <class T, class U>
      bool CheckInvitationExists(T inviter_user_name, U invitee_user_name);
    template <class T, class U>
      bool CheckFriendshipExists(T user_name_1, U user_name_2);
    template <class T>
      bool ValidatePassword(const bsoncxx::document::view user_document_view,
                            T password);
    void NotLoginHandler(void);
    void AfterLoginValidatedHandler(void) const;
    void InvalidInstructionFormatMessagePrinter(void) const;
    bool GetUsername(string& user_name) const;
  private:
    void CreateCollection(const string& collection_name);
    template <class T>
      bool AddNewUser(T user_name, T password);
    template <class T>
      bool AddNewGroup(T group_name, bool is_public);
    template <class T, class U>
      bool AddNewJoinedGroupRecord(T group_name, U user_name);
    template <class T, class U>
      bool AddNewLoginRecord(T user_name, U password);
    template <class T, class U>
      bool AddNewInvitation(T inviter_user_name, U invitee_user_name);
    template <class T, class U>
      bool AddNewFriendship(T user_name_1, U user_name_2);
    template <class T, class U>
      bool AddNewPost(T user_name, U message);
    template <class T>
      bool DeleteUser(T user_name);
    template <class T>
      bool DeleteOneGroup(T group_name);
    template <class T>
      bool DeleteLoginRecordByUsername(T user_name);
    template <class T>
      bool DeleteLoginRecordByToken(T token);
    template <class T, class U>
      bool DeleteOneInvitation(T inviter_user_name, U invitee_user_name);
    template <class T>
      unsigned int DeleteAllInvitations(T user_name);
    template <class T>
      unsigned int DeleteAllFriends(T user_name);
    template <class T>
      unsigned int DeleteAllPosts(T user_name);
  private:
    template <class T>
      bsoncxx::stdx::optional<bsoncxx::document::value> GetUser(
          T user_name);
    mongocxx::cursor GetAllPublicGroups(void);
    mongocxx::cursor GetAllPrivateGroups(void);
    template <class T>
      bsoncxx::stdx::optional<bsoncxx::document::value> GetLoginUserByUsername(
          T user_name);
    template <class T>
      bsoncxx::stdx::optional<bsoncxx::document::value> GetLoginUserByToken(
          T token);
    template <class T>
      mongocxx::cursor GetInvitationsByInviter(T inviter_user_name);
    template <class T>
      mongocxx::cursor GetInvitationsByInvitee(T invitee_user_name);
    template <class T>
      mongocxx::cursor GetAllFriendsofOneUser(T user_name);
    mongocxx::cursor GetAllPosts(const vector<string>& users);
    mongocxx::cursor GetAllPosts(const string& user_name);
  private:
    void Register(char* instruction);
    void Login(char* instruction, const time_t& now);
    void Delete(char* instruction);
    void Logout(char* instruction);
    void ListInvite(char* instruction);
    void ListFriend(char* instruction);
    void ReceivePost(char* instruction);
    void Invite(char* instruction);
    void AcceptInvite(char* instruction);
    void Post(char* instruction);
    void Send(char* instruction);
    void CreateGroup(char* instruction);
    void ListGroup(char* instruction);
    void ListJoined(char* instruction);
    void JoinGroup(char* instruction);
    void SendGroup(char* instruction);
    void MessageParsing(char* instruction, const time_t& now);
};
#endif
