include ../../../inc/common.mk

PPART = ../pslPartition
DIFF = diff -u

test: defaultsTest level0Test level1Test partSize3Test partSize10Test

defaultsTest:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS=''

level0Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=0'

level1Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-outLevels=1'

partSize3Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-partSize=3'

partSize10Test:
	${MAKE} runTest ID=$@ IN=mrna1.psl OPTS='-partSize=10'

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
