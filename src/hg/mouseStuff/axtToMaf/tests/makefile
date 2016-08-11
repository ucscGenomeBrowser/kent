kentSrc = ../../../..
include ../../../../inc/common.mk

axtToMaf=${DESTBINDIR}/axtToMaf

test:   zeroScoreTest 

zeroScoreTest:     mkdirs
	${axtToMaf} input/zeroScore.axt input/rn3.sizes input/hg15.sizes output/zeroScore.maf
	diff expected/zeroScore.maf output

mkdirs:
	mkdir -p output

clean:
	rm -fr output
