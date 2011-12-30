FLAGS = -std=c++0x -Wall
wire: wire.cpp
	g++ $(FLAGS) -o wire wire.cpp
clean:
	rm wire
