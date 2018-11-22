SOURCE_FILE=hw3_0416005.cpp
EXECUTABLE_FILE=hw3_0416005.o
SERVER_CPP_FILE=server.cpp
SERVER_HPP_FILE=server.hpp

CPP=clang++
COMMON_COMPILE_OPTIONS=-Wall -g -std=c++11
MONGOCXX_DRIVER_LINKING_OPTIONS=$(shell pkg-config --libs libmongocxx)
MONGOCXX_DRIVER_COMPILE_OPTIONS=$(shell pkg-config --cflags libmongocxx)
BOTAN_LINKING_OPTIONS=$(shell pkg-config --libs botan-2)
BOTAN_COMPILE_OPTIONS=$(shell pkg-config --cflags botan-2)

server.o: $(SERVER_CPP_FILE) $(SERVER_HPP_FILE)
	$(CPP) \
		$< \
		$(COMMON_COMPILE_OPTIONS) \
		$(MONGOCXX_DRIVER_COMPILE_OPTIONS) \
		$(BOTAN_COMPILE_OPTIONS) \
		-c

main.o: $(SOURCE_FILE)
	$(CPP) \
		$^ \
		$(COMMON_COMPILE_OPTIONS) \
		$(MONGOCXX_DRIVER_COMPILE_OPTIONS) \
		$(BOTAN_COMPILE_OPTIONS) \
		-c -o $@

compile: main.o server.o
	$(CPP) \
		$^ \
		$(BOTAN_LINKING_OPTIONS) \
		$(MONGOCXX_DRIVER_LINKING_OPTIONS) \
		-o server.out \

clean:
	rm *.o *.out
