kentSrc = ../..
A = pslPretty
include $(kentSrc)/inc/userApp.mk

test:: testDnax testEmpty testBad testJustComments

# testRna is missing its test input file rna.psl

testRna::
	${MKDIR} test/output
	pslPretty -long -axt test/input/rna.psl test/input/hCrea.geno test/input/hCrea.mrna test/output/rna.axt

testJustComments::
	${MKDIR} test/output
	pslPretty -long -axt test/input/justComments.psl test/input/mCrea.geno test/input/hCrea.geno test/output/justComments.axt > test/output/justComments.output 2>&1
	cmp test/output/justComments.output test/expected/justComments.output
	cmp test/output/justComments.axt test/expected/justComments.axt

testEmpty::
	${MKDIR} test/output
	pslPretty -long -axt test/input/empty.psl test/input/mCrea.geno test/input/hCrea.geno test/output/empty.axt > test/output/empty.output 2>&1
	cmp test/output/empty.output test/expected/empty.output
	cmp test/output/empty.axt test/expected/empty.axt

testBad::
	${MKDIR} test/output
	-pslPretty -long -axt test/input/bad.psl test/input/mCrea.geno test/input/hCrea.geno test/output/bad.axt > test/output/bad.output 2>&1
	cmp test/output/bad.output test/expected/bad.output
	cmp test/output/bad.axt test/expected/bad.axt

testDnax::
	${MKDIR} test/output
	pslPretty -long -axt test/input/dnax.psl test/input/mCrea.geno test/input/hCrea.geno test/output/dnax.axt
	cmp test/output/dnax.axt test/expected/dnax.axt

testOld:: 
	${MKDIR} output
	pslPretty -long -axt input/test1.psl input/S1.lst input/S2.lst output/test1.axt

clean::
	rm -fr test/output
