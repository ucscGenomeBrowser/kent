# Build all directories in tabFiles

kentSrc = ..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

# note: lists such as the following need to be defined *before*
# they are used in the wildcard target rules.  An oddity of gnu make

# List of all directories to build

ALL_APPS = \
	tabQuery 

all: $(ALL_APPS:%=%.all)

%.all:
	cd $* && echo $* && $(MAKE)

test: $(ALL_APPS:%=%.test)

%.test:
	@test -s $*/tests/makefile && (echo $*; cd $* && $(MAKE) test) || true

clean: $(ALL_APPS:%=%.clean)
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

%.clean:
	cd $* && echo $* && $(MAKE) clean

