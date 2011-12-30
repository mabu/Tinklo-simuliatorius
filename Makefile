FLAGS = -std=c++0x -Wall

all: wire node

common.o: common.h common.cpp
	g++ -c $(FLAGS) common.cpp

wire: wire.o common.o
	g++ -o wire $(FLAGS) wire.o common.o
wire.o: wire.cpp common.o
	g++ -c $(FLAGS) wire.cpp

node: node.o common.o
	g++ -o node $(FLAGS) node.o common.o
node.o:
	g++ -c $(FLAGS) node.cpp

clean:
	rm wire node *.o
