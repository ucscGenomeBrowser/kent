include ../../../inc/common.mk

pslSelect = ${DESTBINDIR}/pslSelect

test: queryPairs

# basic set of tests
queryPairs: mkdirs
	${pslSelect} -queryPairs=input/queryPairs.queries input/queryPairs.in.psl output/queryPairs.out.psl
	diff -u expected/queryPairs.out.psl output

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
