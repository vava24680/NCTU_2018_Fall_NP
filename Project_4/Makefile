SOURCE_FILE=hw4_0416005.cpp
EXECUTABLE_FILE=hw4_0416005.o
SERVER_CPP_FILE=server.cpp
SERVER_HPP_FILE=server.hpp
SERVER_STOMP_CLIENT_CPP_FILE=server_stomp_client.cpp
SERVER_STOMP_CLIENT_HPP_FILE=server_stomp_client.hpp
SERVER_AMQP_CLIENT_CPP_FILE=server_amqp_client.cpp
SERVER_AMQP_CLIENT_HPP_FILE=server_amqp_client.hpp

CPP=clang++
COMMON_COMPILE_OPTIONS=-Wall -g -std=c++11
MONGOCXX_DRIVER_LINKING_OPTIONS=$(shell pkg-config --libs libmongocxx)
MONGOCXX_DRIVER_COMPILE_OPTIONS=$(shell pkg-config --cflags libmongocxx)
BOTAN_LINKING_OPTIONS=$(shell pkg-config --libs botan-2)
BOTAN_COMPILE_OPTIONS=$(shell pkg-config --cflags botan-2)
SIMPLEAMQPCLIENT_LINKING_OPTIONS=$(shell pkg-config --libs libSimpleAmqpClient)
SIMPLEAMQPCLIENT_COMPILE_OPTIONS=$(shell pkg-config --cflags libSimpleAmqpClient)

hw4_0416005.out: server.o server_amqp_client.o main.o
	$(CPP) \
		$^ \
		$(BOTAN_LINKING_OPTIONS) \
		$(MONGOCXX_DRIVER_LINKING_OPTIONS) \
		$(SIMPLEAMQPCLIENT_LINKING_OPTIONS) \
		-o $@

server.o: $(SERVER_CPP_FILE) $(SERVER_HPP_FILE) server_amqp_client.o
	$(CPP) \
		$< \
		$(COMMON_COMPILE_OPTIONS) \
		$(MONGOCXX_DRIVER_COMPILE_OPTIONS) \
		$(SIMPLEAMQPCLIENT_COMPILE_OPTIONS) \
		$(BOTAN_COMPILE_OPTIONS) \
		-c -o $@

server_amqp_client.o: $(SERVER_AMQP_CLIENT_CPP_FILE) $(SERVER_AMQP_CLIENT_HPP_FILE)
	$(CPP) \
		$< \
		$(COMMON_COMPILE_OPTIONS) \
		$(SIMPLEAMQPCLIENT_COMPILE_OPTIONS) \
		-c -o $@

main.o: $(SOURCE_FILE) $(SERVER_HPP_FILE)
	$(CPP) \
		$< \
		$(COMMON_COMPILE_OPTIONS) \
		-c -o $@

.PHONY: clean
clean:
	rm *.o *.out
