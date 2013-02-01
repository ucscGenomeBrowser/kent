# Build all programs in encode directory

# List of directories to build

DIRS = \
	encodeCvTerm \
	encodeMkChangeNotes \
	encodeStatus \
	hgEncodeApi \
	hgEncodeDataVersions \
	hgEncodeVocab \
	mkEncodeFrameset \
	paraMd5sum \
	validateFiles

# broken
	#ldGencodeIntron \
	#regionAgp \

# List of pipeline dirs
PIPELINE_DIRS = \
        encodeLoad \
        encodeValidate \
	../utils/automation

USE_DIRS = $(DIRS)

all:
	@${MAKE} development
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

development:
	@for D in ${PIPELINE_DIRS}; do \
		 (cd $${D} && ${MAKE} development;) \
	done

beta:
	@for D in ${PIPELINE_DIRS}; do \
		(cd $${D} && ${MAKE} beta;)\
	done

prod: 
	@for D in ${PIPELINE_DIRS}; do \
		(cd $${D} && ${MAKE} prod;) \
	done

clean::
	echo nothing to clean in hg/encode/
