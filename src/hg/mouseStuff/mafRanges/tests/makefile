kentSrc = ../../../..
include ../../../../inc/common.mk

MAFRANGES = ${DESTBINDIR}/mafRanges
DIFF = diff

test: clean all

all:
	mkdir -p output
	${MAFRANGES} input/chrM.maf hg17 output/hg17.bed
	${DIFF} expected/hg17.bed output/hg17.bed
	${MAFRANGES} input/chrM.maf hg17 -notAllOGap output/hg17.nao.bed
	${DIFF} expected/hg17.nao.bed output/hg17.nao.bed
	${MAFRANGES} input/chrM.maf hg17 -otherDb=galGal2 \
	  output/hg17.galGal2.bed
	${DIFF} expected/hg17.galGal2.bed output/hg17.galGal2.bed
	${MAFRANGES} input/chrM.maf hg17 -otherDb=galGal2 -notAllOGap \
	  output/hg17.galGal2.nao.bed
	${DIFF} expected/hg17.galGal2.nao.bed output/hg17.galGal2.nao.bed
	${MAFRANGES} input/chrM.maf hg17 -otherDb=danRer1,fr1 \
	  output/hg17.danRer1.fr1.bed
	${DIFF} expected/hg17.danRer1.fr1.bed output/hg17.danRer1.fr1.bed
	${MAFRANGES} input/chrM.maf galGal2 output/galGal2.bed
	${DIFF} expected/galGal2.bed output/galGal2.bed
	${MAFRANGES} input/chrM.maf galGal2 -notAllOGap output/galGal2.nao.bed
	${DIFF} expected/galGal2.nao.bed output/galGal2.nao.bed
	${MAFRANGES} input/chrM.maf galGal2 -otherDb=fr1 output/galGal2.fr1.bed
	${DIFF} expected/galGal2.fr1.bed output/galGal2.fr1.bed
	${MAFRANGES} input/chrM.maf fr1 output/fr1.bed
	${DIFF} expected/fr1.bed output/fr1.bed
	${MAFRANGES} input/chrM.maf canFam1 output/canFam1.bed
	${DIFF} expected/canFam1.bed output/canFam1.bed
	${MAFRANGES} input/chrM.maf canFam1 -notAllOGap output/canFam1.nao.bed
	${DIFF} expected/canFam1.nao.bed output/canFam1.nao.bed

clean:
	rm -rf output

