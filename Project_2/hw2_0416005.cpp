#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>

using namespace std;
using JSON = nlohmann::json;

int main(int argc, char* argv[]) {
   char text[] = R"(
   {
       "Image": {
           "Width":  800,
           "Height": 600,
           "Title":  "View from 15th Floor",
           "Thumbnail": {
               "Url":    "http://www.example.com/image/481989943",
               "Height": 125,
               "Width":  100
           },
           "Animated" : false,
           "IDs": [116, 943, 234, 38793]
       }
   }
  )";
  cout << text << endl;
  return 0;
}
