# Build all directories in utils

all::  utils

utils:
	@for D in $(USE_DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

# List of directories to build
USE_DIRS = $(DIRS)


clean: 
	@for D in ${USE_BINS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done


DIRS = \
	checkAgpAndFa \
	checkDbSync \
	checkGoldDupes \
	checkN \
	checkNt \
	checkOut \
	checkPsl \
	checkQa \
	checkYbr \
	gapper \
	goldToAgp \
	mendMap \
	ntGoldGl \
	ooChains \
	ooCloneEnds \
	ooCloneInfo \
	ooCloneSizes \
	ooCloneSpan \
	ooGreedy \
	ooLiftSpec \
	ooNtInput \
	ooTester \
	ooWeedZero \
	pairedReads \
	psLayout \
	qaGold \
	g2gOverlap \
	g2gSeqOverlap \
	ooSplitFins \
	cmParse \
	cmpMap \
	barger
