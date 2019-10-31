kentSrc = ../..
include ../../inc/common.mk

compile:
	python chromFixer -g build -a hg19.chromAlias.tsv,hg38.chromAlias.tsv -o ${BINDIR}/chromFixer
	chmod a+x ${BINDIR}/chromFixer
