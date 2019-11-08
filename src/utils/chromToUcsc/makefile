kentSrc = ../..
include ../../inc/common.mk

compile:
	python chromToUcsc -g build -a hg19.chromAlias.tsv,hg38.chromAlias.tsv -o ${DESTBINDIR}/chromToUcsc
	chmod a+x ${DESTBINDIR}/chromToUcsc

clean:
	true

test:
	cd tests && make
