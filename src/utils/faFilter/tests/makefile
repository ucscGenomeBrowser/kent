include ../../../inc/common.mk

test: testMinSize testMaxSize testNameWild testMult testUniq

testMinSize: mkdir
	faFilter -minSize=10 input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testMaxSize: mkdir
	faFilter -maxSize=20 input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testNameWild: mkdir
	faFilter -name='foo*baz'  input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testMult: mkdir
	faFilter -minSize=10 -maxSize=20 -name='foo*baz' input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testUniq: mkdir
	cat input/basic.fa input/basic.fa >output/basicDups.fa
	faFilter -uniq output/basicDups.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

clean::
	rm -rf output

mkdir:
	${MKDIR} -p output
