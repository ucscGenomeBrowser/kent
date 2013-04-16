test:   simpleTest biggerTest

PROG=chimpChromContigAgp

simpleTest:     mkdirs
	${PROG} input/chrom.agp input/scaffold.agp output/chromContig.agp
	diff expected/chromContig.agp output
	
biggerTest:     mkdirs
	${PROG} input/chrom2.agp input/scaffold2.agp output/chromContig2.agp
	diff expected/chromContig2.agp output

mkdirs:
	mkdir -p output

clean:
	rm -fr output
