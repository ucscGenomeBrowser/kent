kentSrc = ../..
A = pslPretty
include $(kentSrc)/inc/userApp.mk

test:: testDnax

# testRna is missing its test input file rna.psl

testRna::
	${MKDIR} test/output
	pslPretty -long -axt test/input/rna.psl test/input/hCrea.geno test/input/hCrea.mrna test/output/rna.axt

testDnax::
	${MKDIR} test/output
	pslPretty -long -axt test/input/dnax.psl test/input/mCrea.geno test/input/hCrea.geno test/output/dnax.axt

testOld:: 
	${MKDIR} output
	pslPretty -long -axt input/test1.psl input/S1.lst input/S2.lst output/test1.axt

clean::
	rm -fr test/output
