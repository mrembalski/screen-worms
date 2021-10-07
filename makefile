CLIENTSOURCES = client/screen-worms-client.cpp common/errors.cpp common/texts.cpp client/server.cpp client/gui.cpp
SERVERSOURCES = server/screen-worms-server.cpp common/errors.cpp common/texts.cpp server/udp.cpp server/timer.cpp server/mixed.cpp server/events.cpp

CXX = g++ 
CXXFLAGS =

all: screen-worms-client screen-worms-server

screen-worms-client: 
	$(CXX) $(CLIENTSOURCES) -std=c++17 -Wall -Wextra -O2 -lpthread -o screen-worms-client
	
screen-worms-server: 
	$(CXX) $(SERVERSOURCES) -std=c++17 -Wall -Wextra -O2 -o screen-worms-server

.PHONY: clean
clean:
	rm -rf *.o screen-worms-client screen-worms-server
