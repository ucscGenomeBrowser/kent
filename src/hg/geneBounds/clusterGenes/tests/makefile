

all:

test: refSeqTest refSeqCdsTest

# test files blatRefSeq.gp  ncbiRefSeq.gp
#  chr1:11,689,948-12,078,317 - some basic genes
#    blat NM_138346.1 edited to have shorter CDS, different exon boundries
#    both NM_000302.2 edited so CDS doesn't overlap
#  chr4:71,201,777-71,250,403 - a merged cluster

refSeqTest: mkout
	clusterGenes output/refSeqTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqTest.gl output/refSeqTest.gl

refSeqCdsTest: mkout
	clusterGenes -cds output/refSeqCdsTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/refSeqCdsTest.gl output/refSeqCdsTest.gl

mkout:
	mkdir -p output

clean:
	rm -rf output
