include ../../../inc/common.mk

PSLFILT = ../pslCDnaFilter
DIFF = diff -u --exclude CVS -r

test: overlapTest

overlapTest:
	${MAKE} doFilter name=$@ inPsl=overlap.psl inSizes=overlap.sizes filtArgs='-minCover=0.15 -minId=0.96 -idNearTop=0.001'


# Recursive target:
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
	${PSLFILT} -verbose=1 -dropped=${outDir}/drop.psl -weirdOverlapped=${outDir}/weird.psl input/${inPsl} ${outDir}/keep.psl >${outDir}/filt.out 2>&1
	${DIFF} expected/${name} output/${name}
clean:
	rm -rf output
