CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

all: server.out client.out

server.out: organizer.cpp organizer.hpp
	$(CXX) $(CXXFLAGS) -o server.out organizer.cpp

client.out: player.cpp player.hpp
	$(CXX) $(CXXFLAGS) -o client.out player.cpp

clean:
	rm -f *.out

.PHONY: all clean
