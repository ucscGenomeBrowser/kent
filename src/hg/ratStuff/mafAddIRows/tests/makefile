kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

A = mafAddIRows

test: test1 testGC
	@echo "all tests passed"

test1:
	${A} input/input.maf input/input.2bit out1.maf
	diff out1.maf expected/expected.maf
	rm -f out1.maf

testGC:
	${A} input/inputGC.maf input/input.2bit outGC.maf
	diff outGC.maf expected/expectedGC.maf
	rm -f outGC.maf

clean::
	rm -f out1.maf outGC.maf
