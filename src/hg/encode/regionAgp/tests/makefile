test:   singleNegStrand firstNegStrand \
	        namePrefixTest namePrefixDirTest simpleTest dirTest zeroTest

#VERBOSE=-verbose=3

PROG=regionAgp

singleNegStrand:     mkdirs
	${PROG} ${VERBOSE} -namePrefix=chicken_ -contigFile=input/chicken.contig.tab input/chick.ENm006_6.bed input/chick.chr19.agp output/chicken_ENm006_6.agp
	diff -b expected/chicken_ENm006_6.agp output

firstNegStrand:     mkdirs
	${PROG} ${VERBOSE} -namePrefix=chicken_ -contigFile=input/chicken.contig.tab input/chick.ENm001_1.bed input/chick.chr1.agp output/chicken_ENm001_1.agp
	diff expected/chicken_ENm001_1.agp output

namePrefixTest:     mkdirs
	${PROG} -namePrefix=rat_ input/test.bed input/test.agp output/testNamePrefix.agp
	diff expected/testNamePrefix.agp output

namePrefixDirTest:     mkdirs
	mkdir -p output/mouse
	${PROG} -namePrefix=mouse_ input/testDir.bed input/testDir.agp -dir output/mouse
	cat output/mouse/*.agp | diff - expected/testNamePrefixDir.agp

zeroTest:     mkdirs
	${PROG} input/test0.bed input/test0.agp output/test0.agp
	diff expected/test0.agp output

simpleTest:     mkdirs
	${PROG} input/test.bed input/test.agp output/region.agp
	diff expected/region.agp output
	
dirTest:         mkdirs
	mkdir -p output/org
	${PROG} input/testDir.bed input/testDir.agp -dir output/org
	cat output/org/*.agp | diff - expected/testDir.agp

mkdirs:
	mkdir -p output

clean:
	rm -fr output
