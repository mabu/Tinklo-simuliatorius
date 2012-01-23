FLAGS=-std=c++0x -Wall -O2
SOURCES=common.cpp         \
        Layer.cpp          \
        LinkLayer.cpp      \
        MacSublayer.cpp    \
        NetworkLayer.cpp   \
        Node.cpp           \
        TransportLayer.cpp \
        Fragment.cpp       \
        types.cpp          \

OBJECTS=$(SOURCES:.cpp=.o)
HEADERS=$(SOURCES:.cpp=.h) Frame.h

all: wire node app

wire: common.o wire.cpp
	g++ -o wire $(FLAGS) wire.cpp common.o

common.o: common.cpp
	g++ -c $(FALGS) common.cpp

node: $(OBJECTS) node.cpp
	g++ -o node $(FLAGS) node.cpp $(OBJECTS) -lrt

app: transport_service.o types.o app.cpp
	g++ -o app $(FLAGS) app.cpp transport_service.o types.o

%.o: %.cpp $(HEADERS)
	g++ -c $(FLAGS) $*.cpp

clean:
	rm -f wire node app *.o
