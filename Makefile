CXX = clang++

CXXFLAGS = -fPIC -DNDEBUG -O3 $(shell mapnik-config --cflags) -DURDL_DISABLE_SSL=1 -Iurdl/include

LIBS = -lfreetype $(shell mapnik-config --libs --ldflags) -licuuc -lboost_thread

SRC = $(wildcard *.cpp) urdl/src/urdl.cpp

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

do: clean all deploy
