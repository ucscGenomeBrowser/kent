kentSrc = ../..
include ../../inc/common.mk

compile:
	python chromToUcsc -g build -a hg19.chromAlias.tsv,hg38.chromAlias.tsv -o ${BINDIR}/chromToUcsc
	chmod a+x ${BINDIR}/chromToUcsc

clean:
	true

test:
	cd tests && make
