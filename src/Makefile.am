bin_PROGRAMS = ixmap
ixmap_LDFLAGS = -L../lib
ixmap_CFLAGS = -O2 -I../lib/include
ixmap_DEPENDENCIES = ../lib/libixmap.la
ixmap_SOURCES = main.c thread.c forward.c epoll.c netlink.c tun.c fib.c neigh.c lpm.c hash.c
ixmap_LDADD = -lpthread -lixmap
