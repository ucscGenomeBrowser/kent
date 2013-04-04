kentSrc = ../..
A = faToTwoBit
include $(kentSrc)/inc/userApp.mk
L += -lm ${SOCKETLIB}

clean::
	rm -fr tests/output

test::
	${MKDIR} tests/output
	faToTwoBit tests/input/noDna.fa tests/output/noDna.2bit 2> tests/output/noDna.out
	diff tests/expected/noDna.out tests/output/noDna.out
	twoBitToFa tests/output/noDna.2bit tests/output/noDna.fa
	diff tests/expected/noDna.fa tests/output/noDna.fa
	faToTwoBit tests/input/testN.fa tests/output/testN.2bit
	twoBitToFa tests/output/testN.2bit tests/output/testN.fa
	diff tests/input/testN.fa tests/output/testN.fa
	faToTwoBit tests/input/testMask.fa tests/output/testMask.2bit
	twoBitToFa tests/output/testMask.2bit tests/output/testMask.fa
	diff tests/input/testMask.fa tests/output/testMask.fa
	faToTwoBit -stripVersion tests/input/genbank.fa tests/output/genbank.2bit
	twoBitToFa tests/output/genbank.2bit tests/output/genbank.strip.fa
	diff tests/expected/genbank.fa tests/output/genbank.strip.fa
