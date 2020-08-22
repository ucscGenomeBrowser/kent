kentSrc = ../../..
include ../../../inc/common.mk


all:
	mkdir -p out
	# alas, our automated tests cannot output anything to stderr, as otherwise emails will be sent
	python2 ${DESTBINDIR}/chromToUcsc -i in/hg19.bed -o out/hg19.bed -a hg19.chromAlias.tsv 2> /dev/null
	python2 ${DESTBINDIR}/chromToUcsc  -i in/mm10.bed -o out/mm10.bed -a mm10.chromAlias.tsv 2> /dev/null
	diff out/hg19.bed expected/hg19.bed
	diff out/mm10.bed expected/mm10.bed
	python3 ${DESTBINDIR}/chromToUcsc  -i in/hg19.bed -o out/hg19.bed -a hg19.chromAlias.tsv 2> /dev/null
	python3 ${DESTBINDIR}/chromToUcsc  -i in/mm10.bed -o out/mm10.bed -a mm10.chromAlias.tsv 2> /dev/null
	diff out/hg19.bed expected/hg19.bed
	diff out/mm10.bed expected/mm10.bed
	python3 ${DESTBINDIR}/chromToUcsc  -i in/hg19.bed -o out/hg19.bed -a https://hgdownload.soe.ucsc.edu/goldenPath/hg19/database/chromAlias.txt.gz 2> /dev/null
	diff out/hg19.bed expected/hg19.bed

clean:
	rm -f out/*
