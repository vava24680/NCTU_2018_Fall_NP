#include "./database.hpp"
#include <iostream>

using namespace std;

int main() {
  Database database;
  int operation;
  int collection_number;
  string collection_name;
  vector<string> collection_names;
  do {
    cout << "This is database pre-setup program for NP Project 3" << endl;
    cout << "Enter a number that represents the operation you want to do" << endl;
    cout << "1: Drop all the collections." << endl;
    cout << "2: Choose which collection to drop." << endl;
    cout << R"(3: Create all the collections that "NP Project 3 needs")" << endl;
    cout << "4: Create collection." << endl;
    cout << "0: exit" << endl;
    cin >> operation;
    switch(operation) {
      case 0:
        cout << "Bye!" << endl;
        break;
      case 1:
        cout << "Drop all the collections" << endl;
        database.DropAllCollections();
        break;
      case 2:
        database.ListCollections(collection_names);
        if (collection_names.empty()) {
          cout << "\tNo collection" << endl;
        } else {
          for(unsigned int i = 0; i < collection_names.size(); ++i) {
            cout << "\t" << i << ". Collection name: " << collection_names.at(i)
                << endl;
          }
        }
        cout << "\tPlease enter a number :";
        cin >> collection_number;
        cout << endl;
        if (collection_number < (int)collection_names.size()
            && collection_number >= 0) {
          database.DropCollection(collection_names.at(collection_number));
        }
        break;
      case 3:
        cout << "Create all the collections" << endl;
        database.CreateAllCollections();
        break;
      case 4:
        cout << "Please the collection name :";
        cin >> collection_name;
        cout << endl;
        database.CreateCollection(collection_name);
        break;
      default:
        cout << "Unknown option" << endl;
        break;
    }
    cin.clear();
    cout << "\n\n";
  } while(operation);
  return 0;
}
