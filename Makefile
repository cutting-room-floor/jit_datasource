CXX = clang++

CXXFLAGS = -DMAPNIK_DEBUG -fPIC -O0 -g $(shell mapnik-config --cflags)

LIBS = -lfreetype -lcurl -lyajl $(shell mapnik-config --libs --ldflags) -licuuc

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

install: clean all deploy
