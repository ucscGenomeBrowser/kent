kentSrc = ..
include ../inc/common.mk

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	bfastMafFix \
	bwtFind \
	bwtMake \
	bwtReverse \
	elandToSpl \
	i16Find \
	itsaDump \
	itsaFind \
	itsaMake \
	shortRepeatFind \
	shortRepeatMerge \
	splat \
	splatCheck1 \
	splatCheck2 \
	splatMerge \
	splatTestSet \
	splatToEland \
	splixMake \
	sufaFind \
	sufaMake \
	sufaRepeatFind \
	sufxDump \
	sufxFind \
	sufxMake

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )
