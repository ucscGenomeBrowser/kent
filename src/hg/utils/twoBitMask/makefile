kentSrc = ../../..
A = twoBitMask
include ${kentSrc}/inc/userApp.mk

# reuse twoFitToFa input files. for tests
TEST_IN = ${kentSrc}/utils/twoBitToFa/tests/input

test::
	${MKDIR} tests/output
	twoBitMask ${TEST_IN}/testN.2bit -type=bed /dev/null tests/output/testN_noMask.2bit
	cmp ${TEST_IN}/testN.2bit tests/output/testN_noMask.2bit
	twoBitMask -add ${TEST_IN}/testMask.2bit -type=bed /dev/null tests/output/testMask_sameMask.2bit
	cmp ${TEST_IN}/testMask.2bit tests/output/testMask_sameMask.2bit
	twoBitMask ${TEST_IN}/testMask.2bit -type=bed /dev/null tests/output/testMask_noMask.2bit
	cmp tests/expected/testMask_noMask.2bit tests/output/testMask_noMask.2bit
	twoBitMask ${TEST_IN}/testMask.2bit tests/input/testMask1.bed tests/output/testMask1.2bit
	cmp tests/expected/testMask1.2bit tests/output/testMask1.2bit
	twoBitMask ${TEST_IN}/testMask.2bit tests/input/testMask2.out tests/output/testMask2.2bit
	cmp ${TEST_IN}/testMask.2bit tests/output/testMask2.2bit
	rm -r tests/output

clean::
	rm -fr tests/output
