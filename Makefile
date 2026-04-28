CC = g++
CFLAGS = -Wall -std=c++17

all: manager agent

manager: manager.cpp log.cpp
	$(CC) $(CFLAGS) -o manager manager.cpp log.cpp

agent: agent.cpp log.cpp
	$(CC) $(CFLAGS) -o agent agent.cpp log.cpp

clean:
	rm -f manager agent
