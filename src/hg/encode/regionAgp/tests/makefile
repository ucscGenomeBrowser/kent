test:   simpleTest dirTest

simpleTest:     mkdirs
	${HOME}/bin/${MACHTYPE}/regionAgp input/test.bed input/test.agp output/region.agp
	diff expected/region.agp output
	
dirTest:         mkdirs
	mkdir -p output/org
	${HOME}/bin/${MACHTYPE}/regionAgp input/testDir.bed input/testDir.agp -dir output/org
	cat output/org/*.agp | diff - expected/testDir.agp

mkdirs:
	mkdir -p output

clean:
	rm -fr output
