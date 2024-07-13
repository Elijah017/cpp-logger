CC = g++ --std=c++17
FLAGS =  -o2
SRCDIR = src
OBJDIR = obj

SERVER = logserver
CLIENT = $(OBJDIR)/logclient.o

all: $(SERVER) $(CLIENT)

$(SERVER): $(SRCDIR)/server.cpp
	$(CC) $(FLAGS) $< -o $@

$(CLIENT): $(SRCDIR)/client.cpp
	$(CC) $(FLAGS) $< -c -o $@

clean:
	rm -f $(SERVER) $(CLIENT)
