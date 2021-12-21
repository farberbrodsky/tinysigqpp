CC = g++
CCFLAGS = -Wall -pthread

default:
	@echo "Usage: make example, make lib, make clean"

lib: build/tinysigqpp.so
build/tinysigqpp.so: build/tinysigqpp.o
	$(CC) $(CCFLAGS) -shared -o build/tinysigqpp.so build/tinysigqpp.o

build/tinysigqpp.o: src/tinysigqpp.cpp src/tinysigqpp.hpp
	$(CC) $(CCFLAGS) -fPIC -c src/tinysigqpp.cpp -o build/tinysigqpp.o

example: build/example
build/example: build/example.o build/tinysigqpp.so
	$(CC) $(CCFLAGS) build/example.o build/tinysigqpp.so -o build/example

build/example.o: src/example.cpp src/tinysigqpp.hpp
	$(CC) $(CCFLAGS) -c src/example.cpp -o build/example.o

clean:
	rm -f build/*.o build/tinysigqpp.so build/example
