## 說明
### Requirement
* C++ compiler with C++11 support (`clang++`/`g++` is ok)
  * 若使用的compiler名稱並非`clang++`或使用`g++`，請將兩個Makefile內的`CPP`變數更改為相對應的compiler program name
* [Botan library](https://github.com/randombit/botan)
* [MongoDB C++ Driver](http://mongocxx.org/)
* MongoDB with user management

### Pre-setup
* `DB_setup/database_example.cpp`重新命名為`DB_setup/database.cpp`
* `server_example.cpp`重新命名為`server.cpp`
* `server.cpp`和`DB_setup/database.cpp`有幾個marcos需要更改
  * `MONGODB_HOST`：mongodb所在的domain name或是ip address
  * `MONGODB_PORT`：mongodb使用的port
  * `MONGODB_USER`：mongodb使用者名稱
  * `MONGODB_PASSWORD`：mongodb使用者密碼
  * `MONGODB_DATABASE`：server program要使用的資料庫名稱

* 上述步驟做完後，到`DB_setup`資料夾內
  * 下`make compile` build 資料庫設定程式
  * build完後執行該程式，依照選項內容選擇
* 資料庫設定完成後回到前一層目錄
  * 下`make compile` build server program
  * `./server.out IP選項 Port選項`
    * IP選項
      ```
      -i, --ipv4 ip-address
      ```
    * port選項
      ```
      -p, --port port
      ```
### 架構
```
.
├── DB_setup
│   ├── database_example.cpp # Class implementation
│   ├── database.hpp         # Class definition
│   ├── main.cpp             # main
│   ├── db_setup.out         # ELF file
│   └── Makefile
├── hw3_0416005.cpp          # main program
├── Makefile
├── README.md
├── server.cpp               # Class implementation
├── server.hpp               # Class definition
└── server.out               # ELF file
```
