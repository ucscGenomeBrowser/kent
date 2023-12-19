kentSrc = ../../..
include ../../../inc/common.mk

subColumn = ${DESTBINDIR}/subColumn

all: test

test: passMissing skipMissing

passMissing: output
	${subColumn} -miss=output/outputMiss.bed 1 input/colored.bed input/ensembl.tab output/passMissing.tab
	diff expected/passMissing.tab output/passMissing.tab
	diff expected/outputMiss.bed output/outputMiss.bed

skipMissing: output
	${subColumn} -skipMiss 1 input/colored.bed input/ensembl.tab output/skipMissing.tab
	diff expected/skipMissing.tab output/skipMissing.tab

output:
	${MKDIR} output

clean:
	rm -rf output

