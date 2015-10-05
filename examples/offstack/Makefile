#Makefile
#char code "LF(UNIX)" is required!!

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS =
TARGET = ixmapfwd
LIBS = -lpthread -lixmap
OBJECTS = main.o thread.o forward.o epoll.o netlink.o tun.o fib.o neigh.o lpm.o hash.o

.SUFFIXES: .c .o

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJECTS)
