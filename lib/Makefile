#Makefile
#char code "LF(UNIX)" is required!!

CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared 
TARGET = ixmap
LIBS = -lpthread
OBJECTS = ixmap_common.o ixmap_driver.o ixmap_rxinit.o ixmap_txinit.o
MAJOR = 1
MINOR = 0

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
