include ../../../inc/common.mk

PSLFILT = ../pslCDnaFilter
DIFF = diff -u --exclude CVS -r

test: overlapTest maxAlignTest minQSizeTest

overlapTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl inSizes=overlap.sizes \
	    filtArgs='-minCover=0.15 -minId=0.96 -idNearTop=0.001'

maxAlignTest:
	${MAKE} doFilter name=$@ inPsl=NM_004038.3.psl inSizes=NM_004038.3.sizes \
	    filtArgs='-maxAligns=2 -minCover=0 -minId=0'

minQSizeTest:
	${MAKE} doFilter name=$@ inPsl=misc.psl \
	    filtArgs='-minQSize=30 -minCover=0 -minId=0'


# Recursive targets:
#  o name - test name
#  o filtArgs - filter arguments
#  o inPsl - input psl, relative to input dir
#  o inSize - polyA sizes file, relative to input dir (optional)
ifneq (${inSizes},)
	sizesOpt= -polyASizes=input/${inSizes}
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
