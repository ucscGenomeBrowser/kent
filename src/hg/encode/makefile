# Build all programs in encode directory

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

install:
	cp *.csh *.pl /cluster/data/encode/bin/scripts

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	regionAgp \
	ldGencodeIntron \
	hgEncodeDataVersions

