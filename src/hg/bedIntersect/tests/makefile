kentSrc = ../../..
include ../../../inc/common.mk

bedIntersect = ${DESTBINDIR}/bedIntersect
coverages = 5 10 50 100

all::

test:: default minCov bScore bScorePlusMinCov \
       aHitAny aHitAnyPlusMinCov aHitAnyPlusBScore \
       aHitAnyPlusBScorePlusMinCov \
       allowStartEqEnd allowStartEqEndPlusAHitAny allowStartEqEndPlusAHitAnyPlusBScore

default: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.bed
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.bed
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

minCov: mkout
	for C in ${coverages}; do \
	  ( ${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.$${C}.bed \
	      -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` && \
	    diff expected/$@.aBigBSub.$${C}.bed output/$@.aBigBSub.$${C}.bed && \
	    ${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.$${C}.bed \
	      -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` && \
	    diff expected/$@.aSubBBig.$${C}.bed output/$@.aSubBBig.$${C}.bed ) ; \
	done

bScore: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.bed \
	  -bScore
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -bScore
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

bScorePlusMinCov: mkout
	for C in ${coverages}; do \
	  ( ${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.$${C}.bed \
	      -bScore -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` && \
	    diff expected/$@.aBigBSub.$${C}.bed output/$@.aBigBSub.$${C}.bed && \
	    ${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.$${C}.bed \
	      -bScore -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` && \
	    diff expected/$@.aSubBBig.$${C}.bed output/$@.aSubBBig.$${C}.bed ) ; \
	done

aHitAny: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.bed \
	  -aHitAny
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -aHitAny
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

aHitAnyPlusMinCov: mkout
	for C in ${coverages}; do \
	  ( ${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.$${C}.bed \
	      -aHitAny -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` \
	      2>output/$@.aBigBSub.$${C}.stderr && \
	    diff expected/$@.aBigBSub.$${C}.bed output/$@.aBigBSub.$${C}.bed && \
	    diff expected/$@.aBigBSub.$${C}.stderr output/$@.aBigBSub.$${C}.stderr && \
	    ${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.$${C}.bed \
	      -aHitAny -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` \
	      2>output/$@.aSubBBig.$${C}.stderr && \
	    diff expected/$@.aSubBBig.$${C}.bed output/$@.aSubBBig.$${C}.bed && \
	    diff expected/$@.aSubBBig.$${C}.stderr output/$@.aSubBBig.$${C}.stderr ) ; \
	done

aHitAnyPlusBScore: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.bed \
	  -aHitAny -bScore
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -aHitAny -bScore
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

aHitAnyPlusBScorePlusMinCov: mkout
	for C in ${coverages}; do \
	  ( ${bedIntersect} input/bigBlock.bed input/subBlocks.bed output/$@.aBigBSub.$${C}.bed \
	      -aHitAny -bScore -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` \
	      2>output/$@.aBigBSub.$${C}.stderr && \
	    diff expected/$@.aBigBSub.$${C}.bed output/$@.aBigBSub.$${C}.bed && \
	    diff expected/$@.aBigBSub.$${C}.stderr output/$@.aBigBSub.$${C}.stderr && \
	    ${bedIntersect} input/subBlocks.bed input/bigBlock.bed output/$@.aSubBBig.$${C}.bed \
	      -aHitAny -bScore -minCoverage=`calc $${C} / 100 | awk '{print \$$5;}'` \
	      2>output/$@.aSubBBig.$${C}.stderr && \
	    diff expected/$@.aSubBBig.$${C}.bed output/$@.aSubBBig.$${C}.bed && \
	    diff expected/$@.aSubBBig.$${C}.stderr output/$@.aSubBBig.$${C}.stderr ) ; \
	done

allowStartEqEnd: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocksWInserts.bed output/$@.aBigBSub.bed \
	  -allowStartEqualEnd
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocksWInserts.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -allowStartEqualEnd
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

allowStartEqEndPlusAHitAny: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocksWInserts.bed output/$@.aBigBSub.bed \
	  -allowStartEqualEnd -aHitAny
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocksWInserts.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -allowStartEqualEnd -aHitAny
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

allowStartEqEndPlusAHitAnyPlusBScore: mkout
	${bedIntersect} input/bigBlock.bed input/subBlocksWInserts.bed output/$@.aBigBSub.bed \
	  -allowStartEqualEnd -aHitAny -bScore
	diff expected/$@.aBigBSub.bed output/$@.aBigBSub.bed
	${bedIntersect} input/subBlocksWInserts.bed input/bigBlock.bed output/$@.aSubBBig.bed \
	  -allowStartEqualEnd -aHitAny -bScore
	diff expected/$@.aSubBBig.bed output/$@.aSubBBig.bed

commutativeTest: mkout
	@${bedIntersect} -minCoverage=0.0000000014 input/chrUn_LVXT01056135v1.bed \
		input/chrUn_LVXT01056135v1Gaps.bed stdout | cut -f1-3 > output/$@.first.bed
	@${bedIntersect} -minCoverage=0.0000000014 input/chrUn_LVXT01056135v1Gaps.bed \
		input/chrUn_LVXT01056135v1.bed stdout | cut -f1-3 >  output/$@.second.bed
	@diff output/$@.first.bed output/$@.second.bed

mkout:
	@mkdir -p output

clean::
	rm -rf output
