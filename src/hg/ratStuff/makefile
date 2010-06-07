# Build all directories 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	mafAddIRows \
	mafAddQRows \
	mafFilter \
	mafFrag \
	mafFrags \
	mafGene \
	mafMeFirst \
	mafSpeciesList \
	mafSpeciesSubset \
	mafSplit \
	mafSplitPos \
	mafsInRegion \
	mafOrder \
	stageMultiz 
