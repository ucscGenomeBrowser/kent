kentSrc = ../../..
include ../../../inc/common.mk

pslSelect = ${DESTBINDIR}/pslSelect

test: queryPairs queryPairs2

# basic set of tests
queryPairs: mkdirs
	${pslSelect} -queryPairs=input/queryPairs.queries input/queryPairs.in.psl output/queryPairs.out.psl
	diff -u expected/queryPairs.out.psl output

# with delimiter
queryPairs2: mkdirs
	${pslSelect} -qtPairs=input/queryPairs2.queries input/queryPairs2.in.psl output/queryPairs2.out.psl -qDelim=- -qPass
	
	diff -u expected/queryPairs2.out.psl output

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
