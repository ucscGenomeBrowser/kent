include ../../../../inc/common.mk

all:

test: refSeqTest refSeqCdsTest trackNamesTest selectTrackTest select2TracksTest

# test files blatRefSeq.gp  ncbiRefSeq.gp
#  chr1:11,689,948-12,078,317 - some basic genes
#    blat NM_138346.1 edited to have shorter CDS, different exon boundries
#    both NM_000302.2 edited so CDS doesn't overlap
#  chr4:71,201,777-71,250,403 - a merged cluster

refSeqTest: mkout
	clusterGenes -clusterBed=output/refSeqTest.bed output/refSeqTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqTest.gl output/refSeqTest.gl
	diff -u expected/refSeqTest.bed output/refSeqTest.bed

refSeqCdsTest: mkout
	clusterGenes -cds output/refSeqCdsTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqCdsTest.gl output/refSeqCdsTest.gl

trackNamesTest: mkout
	clusterGenes -cds -trackNames output/trackNamesTest.gl hg16 fred input/blatRefSeq.gp barney input/ncbiRefSeq.gp
	diff -u expected/trackNamesTest.gl output/trackNamesTest.gl

# test requiring one track
selectTrackTest: mkout
	clusterGenes -cds -requiredTracks=ncbiRefSeq output/selectTrackTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/selectTrackTest.gl output/selectTrackTest.gl

# test requiring two tracks
select2TracksTest: mkout
	clusterGenes -cds -trackNames -requiredTracks=barney,wilma output/$@.gl hg16 fred input/blatRefSeq.gp barney input/ncbiRefSeq.gp wilma input/ncbiRefSeqSubset.gp
	diff -u expected/$@.gl output/$@.gl

mkout:
	${MKDIR} output

clean:
	rm -rf output
