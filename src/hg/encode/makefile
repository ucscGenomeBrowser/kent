# Build all programs in encode directory

# List of directories to build

DIRS = \
	hgEncodeDataVersions \
	mkEncodeFrameset 
	hgEncodeVocab \

# broken
	#ldGencodeIntron \
	#regionAgp \

# List of pipeline dirs
PIPELINE_DIRS = \
        encodeValidate \
        encodeLoad \
        encodeUnload

USE_DIRS = $(DIRS)

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

install: 
	chmod +x *.csh *.pl
	cp -p *.csh *.pl /cluster/data/encode/bin/scripts

pipeline:
	@${MAKE} --no-print-directory USE_DIRS="${PIPELINE_DIRS}" all
	
