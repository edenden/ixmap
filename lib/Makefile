#Makefile
#char code "LF(UNIX)" is required!!

CC = gcc
CFLAGS = -Wall -fPIC -O2
LDFLAGS = -shared 
TARGET = ixmap
LIBS = 
MAJOR = 1
MINOR = 0
OBJECTS = ixmap.o driver.o rxinit.o txinit.o

.SUFFIXES: .c .o

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -Wl,-soname=lib$@.so.$(MAJOR) \
	-o lib$@.so.$(MAJOR).$(MINOR) $^ $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f lib*.so.* $(OBJECTS)
