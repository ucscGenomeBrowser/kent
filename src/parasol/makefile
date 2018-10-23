include ../inc/localEnvironment.mk

ifeq (${DESTDIR},)
  DESTDIR=${HOME}/bin
endif
ifeq (${MACHTYPE},)
  MACHTYPE:=$(shell uname -m)
endif
ifeq (${BINDIR},)
  BINDIR=${MACHTYPE}
endif

USE_DIRS = \
	gensub2 \
	para \
	paraHub \
	paraHubStop \
	paraNode \
	paraNodeStart \
	paraNodeStatus \
	paraNodeStop \
	paraTestJob \
	parasol

all: $(USE_DIRS:%=%.all)

lib.all:
	mkdir -p bin
	cd lib && $(MAKE)

# make sure lib is built first, even in a parallel make

%.all: lib.all
	cd $* && echo $* && $(MAKE)

install:
	@echo 'parasol install, please copy ./bin/*  where desired ...'

userApps: all
	rsync -a ./bin/* ${DESTDIR}/${BINDIR}/

clean:
	rm -f */*.o lib/*/*.a
	rm -f bin/*

