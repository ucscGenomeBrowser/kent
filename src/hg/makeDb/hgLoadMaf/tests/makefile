# tests for hgLoadMaf

PROG = hgLoadMaf
P = ${HOME}/bin/${MACHTYPE}/${PROG}

test: testNan 

clean:
	rm -f output/*

testNan:
	@$P -warn -test=input/testNan.maf hg16 testNan > output/testNan.log
	@mv testNan.tab output
	@diff output/testNan.tab expected/testNan.tab >> output/testNan.log

