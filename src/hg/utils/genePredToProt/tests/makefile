kentSrc = ../../../..
include ../../../../inc/common.mk

genePredToProt = ${DESTBINDIR}/genePredToProt
DIFF = diff -u

hg38Genome = /hive/data/genomes/hg38/hg38.2bit

test: refSeqTest refSeqSuffixTest includeStopTest starForInframeStopsTest translateSelnoTest combinedStopOptionsTest
	${MAKE} clean

refSeqTest: mkout
	${genePredToProt} input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa -cdsFa=output/$@.cds.fa
	diff expected/$@.prot.fa output/$@.prot.fa
	diff expected/$@.cds.fa output/$@.cds.fa

refSeqSuffixTest: mkout
	${genePredToProt} -protIdSuffix=_prot -cdsIdSuffix=_cds input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa -cdsFa=output/$@.cds.fa
	diff expected/$@.prot.fa output/$@.prot.fa
	diff expected/$@.cds.fa output/$@.cds.fa

includeStopTest: mkout
	${genePredToProt} -includeStop input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa
	diff expected/$@.prot.fa output/$@.prot.fa

starForInframeStopsTest: mkout
	${genePredToProt} -starForInframeStops input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa
	diff expected/$@.prot.fa output/$@.prot.fa

translateSelnoTest: mkout
	${genePredToProt} -translateSeleno input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa
	diff expected/$@.prot.fa output/$@.prot.fa

combinedStopOptionsTest: mkout
	${genePredToProt} -includeStop -starForInframeStops -translateSeleno input/refseq.grch38.gp ${hg38Genome} output/$@.prot.fa
	diff expected/$@.prot.fa output/$@.prot.fa


mkout:
	@${MKDIR} output

clean:
	rm -rf output
