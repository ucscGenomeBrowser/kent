kentSrc = ../../..
include ../../../inc/common.mk

faFilter=${DESTBINDIR}/faFilter

test: testMinSize testMaxSize testNameWild testMult testUniq testUniqIc testPatList

testMinSize: mkdir
	${faFilter} -minSize=10 input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testMaxSize: mkdir
	${faFilter} -maxSize=20 input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testNameWild: mkdir
	${faFilter} -name='foo*baz'  input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testMult: mkdir
	${faFilter} -minSize=10 -maxSize=20 -name='foo*baz' input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testUniq: mkdir
	cat input/basic.fa input/basic.fa >output/basicDups.fa
	${faFilter} -uniq output/basicDups.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testUniqIc: mkdir
	${faFilter} -uniq -i input/uniqIc.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

testPatList: mkdir
	${faFilter} -namePatList=input/basicIds.lst input/basic.fa output/$@.fa
	diff -u expected/$@.fa output/$@.fa

	
clean::
	rm -rf output

mkdir:
	${MKDIR} -p output
