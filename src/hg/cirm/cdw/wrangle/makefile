#Borrowing a bit of code from one of Hiram's makefiles (src/utils)
WRANGLE_APPLIST = \
	addMetaDataToJson\
	cdwSnyderomeStepLink\
	cdwWrangleIsaacInput\
	condenseMatrix\
	kallistoOnFastqMani\
	makeExpMatrix\
	matrixFilter\
	rnaSeqToClustersPipeline\
	kallistoToMatrix

# List of all directories to build

ALL_APPS = $(WRANGLE_APPLIST)

# all: builds all utilities here

all: $(ALL_APPS:%=%.all)

%.all:
	cd $* && echo $* && $(MAKE)
