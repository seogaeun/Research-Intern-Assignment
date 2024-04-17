CC=gcc
CFLAGS = -g -Wall
OBJS=server.o client.o 
TARGET=server.out


all: client server

clean:
	rm -f hw1 hw1_sol



$(TARGET): $(OBJS)
    $(CC) -o $@ $(OBJS)

server.o : server.c
    $(CC) -c -o server.o server.c

client.o : client.c
    $(CC) -c -o client.o client.c

clean:
    rm $(OBJECT) $(TARGET)