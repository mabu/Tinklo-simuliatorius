FLAGS=-std=c++0x -Wall -O2
SOURCES=common.cpp         \
        Layer.cpp          \
        LinkLayer.cpp      \
        MacSublayer.cpp    \
        NetworkLayer.cpp   \
        Node.cpp           \
        TransportLayer.cpp \
        types.cpp          \

OBJECTS=$(SOURCES:.cpp=.o)
HEADERS=$(SOURCES:.cpp=.h) Frame.h

all: wire node

wire: wire.o common.o
	g++ -o wire $(FLAGS) wire.o common.o

wire.o: wire.cpp common.o
	g++ -c $(FLAGS) wire.cpp

node: $(OBJECTS) node.cpp
	g++ -o node $(FLAGS) node.cpp $(OBJECTS) -lrt

%.o: %.cpp $(HEADERS)
	g++ -c $(FLAGS) $*.cpp

clean:
	rm -f wire node *.o
