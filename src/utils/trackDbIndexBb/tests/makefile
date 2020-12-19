kentSrc = ../../..
include ../../../inc/common.mk


all:
	mkdir -p out
	# alas, our automated tests cannot output anything to stderr, as otherwise emails will be sent
	python2 ${DESTBINDIR}/trackDbIndexBb uniformDnase in/trackDb.smallExample.ra \
in/hg19.chrom.sizes -o out/ -p /cluster/bin/bedtools/bedtools -m metadata -s lab 2> /dev/null
	diff out/uniformDnase.multiBed.bb expected/uniformDnase.multiBed.bb
	diff out/uniformDnase.multiBedSources.tab expected/uniformDnase.multiBedSources.tab
	python3 ${DESTBINDIR}/trackDbIndexBb uniformDnase in/trackDb.smallExample.ra \
in/hg19.chrom.sizes -o out/ -p /cluster/bin/bedtools/bedtools -m metadata -s lab 2> /dev/null
	diff out/uniformDnase.multiBed.bb expected/uniformDnase.multiBed.bb
	diff out/uniformDnase.multiBedSources.tab expected/uniformDnase.multiBedSources.tab

clean:
	rm -f out/*
