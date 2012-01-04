CXX = clang++

CXXFLAGS = -fPIC -O3 $(shell mapnik-config --cflags)

LIBS = -lcurl -lyajl $(shell mapnik-config --libs --ldflags) -licuuc

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
	cp jit.input $(shell mapnik-config --input-plugins)

test:
	./tile.js

do: clean all deploy test
