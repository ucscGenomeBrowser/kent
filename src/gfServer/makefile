kentSrc = ..
A = gfServer
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

test::
	${MKDIR} tests/output
	${DESTBINDIR}/${A} direct tests/input/mCrea.mrna tests/input/hCreaGeno.nib \
		tests/input/mCreaGeno.nib > tests/output/testNib.out
	diff tests/expected/testNib.out tests/output/testNib.out
	${DESTBINDIR}/${A} direct tests/input/mCrea.mrna tests/input/creaGeno.2bit \
		>tests/output/testTwoBit.out
	diff tests/expected/testTwoBit.out tests/output/testTwoBit.out
	cd tests && ./testProtNib
	cd tests && ./testProtTwoBit
	cd tests && ./testTransNib
	cd tests && ./testTransTwoBit
	cd tests && ./testPcr
	cd tests && ./testIndexFile
	cd tests && ./testIndexFileTrans
	cd tests && ./testDynServer
	cd tests && ./testDynServerTrans
	cd tests && ./testDynServerWithDir
	cd tests && ./testDynServerPerSeqMax
	cd tests && ./testDynServerPcr

clean::
	rm -fr ${O} tests/output
