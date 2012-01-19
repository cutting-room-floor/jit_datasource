CXX = clang++

CXXFLAGS = $(shell mapnik-config --cflags)
LDFLAGS = $(shell mapnik-config --libs --dep-libs --ldflags)

OBJ = test.o

BIN = test

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $@

.c.o:
	$(CXX) -c $(CXXFLAGS) $<

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
	rm -f demo.png

dotest:
	./test
	open demo.png

.PHONY: clean test dotest
