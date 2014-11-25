#Makefile
#char code "LF(UNIX)" is required!!

CC = gcc
CFLAGS = -Wall
LDFLAGS =
TARGET = ixmapfwd
LIBS = -lpthread
OBJECTS = main.o forward.o

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
