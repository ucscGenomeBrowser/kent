kentSrc = ../../..
include ${kentSrc}/inc/common.mk

pslSpliceJunctions = ${DESTBINDIR}/pslSpliceJunctions
hg38twobit = /hive/data/genomes/hg38/hg38.2bit

all:

test:: rnahg38Test


rnahg38Test: mkdirs
	${pslSpliceJunctions} input/rna-hg38.psl ${hg38twobit} output/$@.tsv
	diff -u expected/$@.tsv output/$@.tsv


mkdirs:
	@mkdir -p output

clean:
	rm -rf output
