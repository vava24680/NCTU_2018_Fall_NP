MAIN_SOURCE_FILE=main.cpp
DATABASE_SOURCE_CPP=database.cpp
DATABASE_SOURCE_HPP=database.hpp
CPP=clang++
COMMON_COMPILE_OPTIONS=-Wall -g -std=c++11
MONGOCXX_DRIVER_COMPILE_OPTIONS=$(shell pkg-config --cflags --libs libmongocxx)

database.o: $(DATABASE_SOURCE_CPP) $(DATABASE_SOURCE_HPP)
	$(CPP) \
		$(COMMON_COMPILE_OPTIONS) \
		$< \
		-c

main.o: $(MAIN_SOURCE_FILE)
	$(CPP) \
	   $(COMMON_COMPILE_OPTIONS) \
	   $< \
	   -c

compile: main.o database.o
	$(CPP) \
		$(COMMON_COMPILE_OPTIONS) \
		$(MONGOCXX_DRIVER_COMPILE_OPTIONS) \
		$^ \
		-o db_setup.out

clean:
	rm *.out *.o
