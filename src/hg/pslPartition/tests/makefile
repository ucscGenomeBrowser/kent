kentSrc = ../../..
include ../../../inc/common.mk

PPART = ${DESTBINDIR}/pslPartition
DIFF = diff

test: defaultsTest level0Test level1Test partSize3Test partSize10Test dropContainedTest

# not really the default any more
defaultsTest:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=3 -partSize=0'

level0Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=0 -partSize=0'

level1Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=1 -partSize=0'

partSize3Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=3 -partSize=3'

partSize10Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=3 -partSize=10'

# test of dropping PSLs completely contained in the exon of another PSL'
dropContainedTest:
	${MAKE} runTest ID=$@ IN=contained.psl OPTS='-partSize=0 -dropContained'

# recursive target to execute a test
# arguments: ID, IN, OPTS
runTest:
	@${MKDIR} output
	rm -rf output/${ID}.out
	${PPART} ${OPTS} input/${IN} output/${ID}.out
	./getQNames `find output/${ID}.out -type f | sort` >output/${ID}.lst
	${DIFF} expected/${ID}.lst output/${ID}.lst


clean:
	rm -rf output
