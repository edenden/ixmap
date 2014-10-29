#Makefile
#char code "LF(UNIX)" is required!!

OPTIONS = -g -Wall
PROGS = ixgbe

all: $(PROGS)

clean :
	rm -f $(PROGS)
	rm -f *.o

ixgbe : main.o forward.o
	$(CC) $(OPTIONS) -o $@ main.o forward.o -l pthread
	
main.o : main.c
	$(CC) $(OPTIONS) -c main.c

forward.o : forward.c
	$(CC) $(OPTIONS) -c forward.c
