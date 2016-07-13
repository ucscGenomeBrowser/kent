kentSrc = ../../..
include ../../../inc/common.mk

PSLFILT = ${DESTBINDIR}/pslCDnaFilter
DIFF = diff -u --exclude='.nfs*' -r

test:	noopTest overlapTest maxAlignTest minQSizeTest \
	repMatchTest minAlnSizeTest nonRepSizeTest nonRepMismatchTest \
	noDropCntBugTest minSpan100Test minSpan10Test localBestTest \
	globalBestTest ignoreNsCoverTest ignoreNsMinAlnTest \
	ignoreNsLocalBestTest hapGlobalTest hapLocalTest hapLocalXenoTest \
	weirdOverlappedFilterTest weirdOverlapMultAlnTest weirdOverlapMultAlnMinCoverTest \
	noIgnoreIntronsTest ignoreIntronsTest \
	uniqueMappedTest uniqueMappedHapTest blackListTest \
	decayMinCoverTest inconsistentQSizeTest repsAsMatchTest

# nothing should be filtered with no arguments
noopTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl filtArgs=''

overlapTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl inSizes=overlap.sizes \
	    filtArgs='-bestOverlap -minCover=0.15 -minId=0.96'

maxAlignTest:
	${MAKE} doFilter name=$@ inPsl=NM_004038.3.psl inSizes=NM_004038.3.sizes \
	    filtArgs='-maxAligns=2'

minQSizeTest:
	${MAKE} doFilter name=$@ inPsl=misc.psl filtArgs='-minQSize=30'

repsAsMatchTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl filtArgs='-repsAsMatch'

repMatchTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl filtArgs='-maxRepMatch=0.2'

minAlnSizeTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl filtArgs='-minAlnSize=50'

nonRepSizeTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl filtArgs='-minNonRepSize=30'

# check that mismatches are not counted in -minNonRepSize test, as blat doesnt'
# include this in counts
nonRepMismatchTest:
	${MAKE} doFilter name=$@ inPsl=repMatch.psl filtArgs='-minNonRepSize=16'

# file that causes the count of queries kept to be one more than total
noDropCntBugTest:
	${MAKE} doFilter name=$@ inPsl=noDropCntBug.psl filtArgs=''

# minSpan tests
minSpan100Test:
	${MAKE} doFilter name=$@ inPsl=pseudoRetro.psl filtArgs='-minSpan=1.0'

minSpan10Test:
	${MAKE} doFilter name=$@ inPsl=pseudoRetro.psl filtArgs='-minSpan=0.1'

# local best in genome
localBestTest:
	${MAKE} doFilter name=$@ inPsl=many.psl filtArgs='-localNearBest=0.01'

# global best in genome
globalBestTest:
	${MAKE} doFilter name=$@ inPsl=many.psl filtArgs='-globalNearBest=0.01'

# ignore Ns in cover and score
ignoreNsCoverTest:
	${MAKE} doFilter name=$@ inPsl=gapOver.psl filtArgs='-ignoreNs -minCover=0.4'

ignoreNsMinAlnTest:
	${MAKE} doFilter name=$@ inPsl=gapOver.psl filtArgs='-ignoreNs -minAlnSize=80'

ignoreNsLocalBestTest:
	${MAKE} doFilter name=$@ inPsl=gapOver.psl filtArgs='-ignoreNs -localNearBest=0.005'

# resting of haplotype region handling and globalNearBest
hapGlobalTest:
	${MAKE} doFilter name=$@ inPsl=haplotype.psl inSizes=haplotype.sizes filtArgs='-globalNearBest=0.005 -hapRegions=input/hapMappings.psl -hapLociAlns=output/$@/hapLoci.id-psl'

# resting of haplotype region handling and localNearBest
hapLocalTest:
	${MAKE} doFilter name=$@ inPsl=haplotype.psl inSizes=haplotype.sizes filtArgs='-localNearBest=0.005 -hapRegions=input/hapMappings.psl -hapLociAlns=output/$@/hapLoci.id-psl'

# haplotype test using xeno alignments; was bug with comparing with wrong
# refChrom
hapLocalXenoTest:
	${MAKE} doFilter name=$@ inPsl=xenoHap.psl inSizes=haplotype.sizes filtArgs='-localNearBest=0.005 -hapRegions=input/hapMappings.psl -hapLociAlns=output/$@/hapLoci.id-psl'


# filter weird overlapping
weirdOverlappedFilterTest:
	${MAKE} doFilterSave name=$@ inPsl=overlap.psl inSizes=overlap.sizes \
	    filtArgs='-filterWeirdOverlapped'

# Multiple indentical overlapping alignments create by mapping proteins
# via mRNAs, along with a couple of weird alignments.  This results in
# all alignments being kept.  The weird overlap was moved until later
# so that -bestOverlap and other filters still work.
weirdOverlapMultAlnTest:
	${MAKE} doFilterSave name=$@ inPsl=weirdOverlapMultAln.psl \
	    filtArgs='-bestOverlap'

# This test also drops lower quality weird overlaps before weird overlap test
# done.
weirdOverlapMultAlnMinCoverTest:
	${MAKE} doFilterSave name=$@ inPsl=weirdOverlapMultAln.psl \
	    filtArgs='-bestOverlap -minCover=1.0'

# -ignoreIntrons tests, carefully crafted from same alignment so that one with
# faked intron scores slighly lower without intron bonus
noIgnoreIntronsTest:
	${MAKE} doFilterSave name=$@ inPsl=ignoreIntrons.psl filtArgs='-maxAligns=1'

ignoreIntronsTest:
	${MAKE} doFilterSave name=$@ inPsl=ignoreIntrons.psl filtArgs='-maxAligns=1 -ignoreIntrons'

# uniquely mapping
uniqueMappedTest:
	${MAKE} doFilter name=$@ inPsl=many.psl filtArgs='-globalNearBest=0.01 -uniqueMapped'

uniqueMappedHapTest:
	${MAKE} doFilter name=$@ inPsl=haplotype.psl inSizes=haplotype.sizes filtArgs='-uniqueMapped -localNearBest=0.005 -hapRegions=input/hapMappings.psl'

decayMinCoverTest:
	${MAKE} doFilter name=$@ inPsl=decayMinCover.psl filtArgs='-decayMinCover'

blackListTest:
	${MAKE} doFilter name=$@ inPsl=blackList.psl filtArgs='-blackList=input/blackList.txt'

# test for catching PSLs with inconsistent qSize
inconsistentQSizeTest:
	@mkdir -p output
	if ${PSLFILT} input/inconsistentQSize.psl /dev/null 2> output/$@.err ; then  echo "Error: should have failed" >&2; false ; else true ; fi
	diff expected/$@.err output/$@.err

####
# Recursive targets.  Results are sorted to allow consistent comparisons, as
# sometimes internal sorting done by pslCDnaFilter is not stable.
#  o name - test name
#  o filtArgs - filter arguments
#  o inPsl - input psl, relative to input dir
#  o inSize - polyA sizes file, relative to input dir (optional)
ifneq (${inSizes},)
	sizesOpt=-polyASizes=input/${inSizes}
endif
outDir=output/${name}
sortPsl=sort -k 10,10 -k 12,12n -k 13,13n -k 14,14 -k 16,16n -k 17,17n
doFilter:
	@${MKDIR} ${outDir}
	${PSLFILT} ${filtArgs} -verbose=1 input/${inPsl} ${outDir}/keep.psl.tmp >${outDir}/filt.out 2>&1
	@${sortPsl} ${outDir}/keep.psl.tmp > ${outDir}/keep.psl
	@rm -f ${outDir}/keep.psl.tmp
	${DIFF} expected/${name} output/${name}

# saves dropped and weirdOverlapp, also tests -statsOut
doFilterSave:
	@${MKDIR} ${outDir}
	${PSLFILT} ${filtArgs} -verbose=1 -statsOut=${outDir}/filt.out -dropped=${outDir}/drop.psl.tmp -weirdOverlapped=${outDir}/weird.psl.tmp input/${inPsl} ${outDir}/keep.psl.tmp
	@${sortPsl} ${outDir}/keep.psl.tmp > ${outDir}/keep.psl
	@${sortPsl} ${outDir}/drop.psl.tmp > ${outDir}/drop.psl 
	@${sortPsl} ${outDir}/weird.psl.tmp > ${outDir}/weird.psl
	@rm -f ${outDir}/keep.psl.tmp ${outDir}/drop.psl.tmp ${outDir}/weird.psl.tmp
	${DIFF} expected/${name} output/${name}
clean:
	rm -rf output
