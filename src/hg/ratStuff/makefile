kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk

# List of directories to build for userApps
UTILS_APPLIST = \
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
	mafOrder

# List of directories to build at UCSC
DIRS = \
	stageMultiz

# List of directories to build
USE_DIRS = $(DIRS) $(UTILS_APPLIST)

# Build all directories
all: ${USE_DIRS:%=%.all}
%.all:
	cd $* && echo $* && ${MAKE}

test: ${USE_DIRS:%=%.test}
%.test:
	cd $* && echo $* && ${MAKE} test

userApps: $(UTILS_APPLIST:%=%.userApp)
%.userApp:
	cd $* && echo $* && $(MAKE)

clean: $(USE_DIRS:%=%.clean)
%.clean:
	cd $* && echo $* && $(MAKE) clean
