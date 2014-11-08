#Makefile
#char code "LF(UNIX)" is required!!

CC = gcc
CFLAGS = -Wall
LDFLAGS =
TARGET = ixgbe
LIBS = pthread
OBJECTS = main.o forward.o rxinit.o txinit.o

.SUFFIXES: .c .o

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -l $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJECTS)
