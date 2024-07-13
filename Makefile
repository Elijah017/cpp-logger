CC = g++ --std=c++17
FLAGS =  -o2
SRCDIR = src
OBJDIR = obj

SERVER = logserver
LOG_O = $(OBJDIR)/logserver.o
CLIENT = $(OBJDIR)/logclient.o

all: $(SERVER) $(CLIENT)

$(SERVER): $(SRCDIR)/main.cpp $(LOG_O)
	$(CC) $(FLAGS) $^ -o $@

$(LOG_O): $(SRCDIR)/server.cpp
	$(CC) $(FLAGS) $^ -o $@ -c

$(CLIENT): $(SRCDIR)/client.cpp
	$(CC) $(FLAGS) $^ -o $@ -c

clean:
	rm -f $(SERVER) $(OBJDIR)/*
