kentSrc = ../../..
include ../../../inc/common.mk

# test files were made with:
#
#bigWigToBedGraph /gbdb/mm10/encode3/atac/encode3RenAtacSignalStomachE16.bw stdout | randomLines stdin 20 stdout > mm10.bed
#bedSort mm10.bed mm10.bed
#bedGraphToBigWig mm10.bed /hive/data/genomes/mm10/chrom.sizes mm10.bw

#bigBedToBed /gbdb/hg19/bbi/clinvar/clinvarCnv.bb stdout  | cut -f1-4 | randomLines stdin 20 stdout > hg19.bed
#bedSort hg19.bed hg19.bed
#bedToBigBed hg19.bed /hive/data/genomes/hg19/chrom.sizes hg19.bb

all:
	mkdir -p out
	# alas, our automated tests cannot output anything to stderr, as otherwise emails will be sent
	rm -f out/test.err
	python2 ${DESTBINDIR}/bigGuessDb --indexFile ./guessAssembly.txt.gz in/hg19.bb > out/hg19.txt
	python3 ${DESTBINDIR}/bigGuessDb --indexFile ./guessAssembly.txt.gz in/mm10.bw > out/mm10.txt
	python3 ${DESTBINDIR}/bigGuessDb --indexFile ./guessAssembly.txt.gz in/mm10.bw --best > out/mm10.best.txt
	diff out/hg19.txt expected/hg19.txt
	diff out/mm10.txt expected/mm10.txt
	diff out/mm10.best.txt expected/mm10.best.txt

test: all

clean:
	rm -f out/*
