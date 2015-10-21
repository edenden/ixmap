ACLOCAL_AMFLAGS = -I m4
SUBDIRS = lib src
DIST_SUBDIRS = $(SUBDIRS)

all-local:
	cd ./kmod; make

install-exec-local:
	cd ./kmod; make install
# depmod.sh in kernel build system doesn't invoke
# /sbin/depmod if System.map is not found in build directory.
# So we call /sbin/depmod explicitly as a work-around.
# Ref: /usr/lib/linux-kbuild-X.XX/scripts/depmod.sh
	/sbin/depmod -a

clean-local:
	cd ./kmod; make clean
