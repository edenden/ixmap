ACLOCAL_AMFLAGS = -I m4
SUBDIRS = lib src
DIST_SUBDIRS = $(SUBDIRS)

all-local:
	cd ./kmod; make

install-exec-local:
	cd ./kmod; make install

clean-local:
	cd ./kmod; make clean
