test:   mkdirs
	${HOME}/bin/${MACHTYPE}/regionAgp input/test.bed input/test.agp output/region.agp
	diff expected/region.agp output

mkdirs:
	mkdir -p output

clean:
	rm -fr output
