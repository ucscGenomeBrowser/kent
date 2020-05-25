kentSrc = ../../..
include ../../../inc/common.mk


all:
	#mkdir -p out
	# alas, our automated tests cannot output anything to stderr, as otherwise emails will be sent
	python2 trackDbIndexBb -t uniformDnase -r in/trackDb.smallExample.txt -c in/hg19.chrom.sizes -o out/ -p /cluster/bin/bedtools/bedtools -m metadata -s lab 2> /dev/null
	diff out/uniformDnase.multiBed.bb expected/uniformDnase.multiBed.bb
	diff out/uniformDnase.multiBedSources.tab expected/uniformDnase.multiBedSources.tab
	python3 trackDbIndexBb -t uniformDnase -r in/trackDb.smallExample.txt -c in/hg19.chrom.sizes -o out/ -p /cluster/bin/bedtools/bedtools -m metadata -s lab 2> /dev/null
	diff out/uniformDnase.multiBed.bb expected/uniformDnase.multiBed.bb
	diff out/uniformDnase.multiBedSources.tab expected/uniformDnase.multiBedSources.tab
	rm -f out/*

clean:
	rm -f out/*
