include ../../../inc/common.mk

DIFF = diff -u
genePredToGtf = ${BINDIR}/genePredToGtf

##
# Note: the target bigTest runs this on all mRNAs however some fail because
# gtfToGenePred doesn't handled splcied stop codons.
##

# special cases:
#   - NM_021209 has a spliced start codon
#   - NM_001018001 marked 3' incomplete, positive strand
#   - D31736 marked 3' incomplete, negative strand.  CDS only contains
#     start codon.
#   - AF409062 has a spliced stop codon

test: \
	basicSimpleTest utrSimpleTest \
	basicTest utrTest honorTest

# tests using basic genePred
basicSimpleTest: mkout
	${genePredToGtf} -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrSimpleTest: mkout
	${genePredToGtf} -utr -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf


# tests using extended genePred
basicTest: mkout
	${genePredToGtf} -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrTest: mkout
	${genePredToGtf} -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

honorTest: mkout
	${genePredToGtf} -honorCdsStat -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

mkout:
	@${MKDIR} output

clean:
	rm -rf output bigout

.SECONDARY:
bigTest: bigout/bigTest.diff

bigout/bigTest.diff: bigout/bigTest.orig.gp bigout/bigTest.recreate.gp
	diff $^ >$@.tmp
	mv -f $@.tmp $@

# need to drop _2, etc modifiers added wheren their are duplicate alignments
bigout/bigTest.recreate.gp: bigout/bigTest.orig.gtf
	gtfToGenePred $< stdout | tawk '{$$1=gensub("_[0-9]+$$", "", "g", $$1); print $$0}' >$@.tmp
	mv -f $@.tmp $@

bigout/bigTest.orig.gtf: bigout/bigTest.orig.gp
	genePredToGtf file $< $@.tmp
	mv -f $@.tmp $@

bigout/bigTest.orig.gp:
	@mkdir -p $(dir $@)
	mrnaToGene -db=hg18 -insertMergeSize=8 all_mrna $@.tmp
	mv -f $@.tmp $@
