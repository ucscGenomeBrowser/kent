# Build all programs in encode directory

# List of directories to build

DIRS = \
	hgEncodeDataVersions \
	mkEncodeFrameset \
	hgEncodeVocab \
	validateFiles \
        encodeMkChangeNotes \
        paraMd5sum

# broken
	#ldGencodeIntron \
	#regionAgp \

# List of pipeline dirs
PIPELINE_DIRS = \
        encodeLoad \
        encodeStatus \
        encodeValidate

USE_DIRS = $(DIRS)

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

install: 
	@for D in ${PIPELINE_DIRS}; do \
		(cd $${D} && ${MAKE} install;) \
	done

pipeline:
	@${MAKE} --no-print-directory USE_DIRS="${PIPELINE_DIRS}" all

clean::
	echo nothing to clean in hg/encode/
