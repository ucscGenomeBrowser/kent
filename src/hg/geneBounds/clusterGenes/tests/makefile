

all:

test: refSeqTest refSeqCdsTest

# test files blatRefSeq.gp  ncbiRefSeq.gp
#  chr1:11,689,948-12,078,317 - some basic genes
#    blat NM_138346.1 edited to have shorter CDS, different exon boundries
#    both NM_000302.2 edited so CDS doesn't overlap
#  chr4:71,201,777-71,250,403 - a merged cluster

refSeqTest: mkout
	clusterGenes -conflicts=output/refSeqTest.con output/refSeqTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqTest.gl output/refSeqTest.gl
	diff -u expected/refSeqTest.con output/refSeqTest.con

refSeqCdsTest: mkout
	clusterGenes -cds -conflicts=output/refSeqCdsTest.con output/refSeqCdsTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqCdsTest.con output/refSeqCdsTest.con

mkout:
	mkdir -p output

clean:
	rm -rf output
