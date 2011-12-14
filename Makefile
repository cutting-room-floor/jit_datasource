CXX = g++

CXXFLAGS = -fPIC -O3 -Isqlite_sources/ $(shell mapnik-config --cflags)

LIBS = -lsqlite3 -lcurl -lyajl $(shell mapnik-config --libs --ldflags) -licuuc

SRC = $(wildcard *.cpp)

OBJ = $(SRC:.cpp=.o)

BIN = jit.input

all : $(SRC) $(BIN)

$(BIN) : $(OBJ)
	$(CXX) -shared $(OBJ) $(LIBS) -o $@  

.cpp.o :
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY : clean

clean: 
	rm -f $(OBJ)
	rm -f $(BIN)

deploy:
	cp jit.input /usr/local/lib/mapnik/input/

do: clean all deploy
