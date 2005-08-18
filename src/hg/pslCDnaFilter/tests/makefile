include ../../../inc/common.mk

PSLFILT = ../pslCDnaFilter
DIFF = diff -u --exclude CVS -r

test:	noopTest overlapTest maxAlignTest minQSizeTest \
	nonCompareTest repMatchTest noDropCntBugTest \
	minSpan100Test minSpan10Test


# nothing should be filtered with no arguments
noopTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl filtArgs=''

overlapTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl inSizes=overlap.sizes \
	    filtArgs='-bestOverlap -minCover=0.15 -minId=0.96 -idNearTop=0.001'

maxAlignTest:
	${MAKE} doFilter name=$@ inPsl=NM_004038.3.psl inSizes=NM_004038.3.sizes \
	    filtArgs='-maxAligns=2'

minQSizeTest:
	${MAKE} doFilter name=$@ inPsl=misc.psl \
	    filtArgs='-minQSize=30'

nonCompareTest:
	${MAKE} doFilter name=$@ inPsl=NM_004038.3.psl inSizes=NM_004038.3.sizes \
	    filtArgs='-nonComparative -minCover=0.15 -minId=0.96'

repMatchTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl \
	    filtArgs='-maxRepMatch=0.2'

# file that causes the count of queries kept to be one more than total
noDropCntBugTest:
	${MAKE} doFilter name=$@ inPsl=noDropCntBug.psl filtArgs=''

# minSpan tests
minSpan100Test:
	${MAKE} doFilter name=$@ inPsl=pseudoRetro.psl filtArgs='-minSpan=1.0'

minSpan10Test:
	${MAKE} doFilter name=$@ inPsl=pseudoRetro.psl filtArgs='-minSpan=0.1'


# Recursive targets:
#  o name - test name
#  o filtArgs - filter arguments
#  o inPsl - input psl, relative to input dir
#  o inSize - polyA sizes file, relative to input dir (optional)
ifneq (${inSizes},)
	sizesOpt=-polyASizes=input/${inSizes}
endif
outDir=output/${name}
doFilter:
	@${MKDIR} ${outDir}
	${PSLFILT} ${filtArgs} -verbose=1 input/${inPsl} ${outDir}/keep.psl >${outDir}/filt.out 2>&1
	${DIFF} expected/${name} output/${name}

# saves dropped and weirdOverlapp
doFilterSave:
	@${MKDIR} ${outDir}
	${PSLFILT} ${filtArgs} -verbose=1 -dropped=${outDir}/drop.psl -weirdOverlapped=${outDir}/weird.psl input/${inPsl} ${outDir}/keep.psl >${outDir}/filt.out 2>&1
	${DIFF} expected/${name} output/${name}
clean:
	rm -rf output
