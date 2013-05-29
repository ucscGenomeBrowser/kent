kentSrc = ..
A = gfServer
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

test::
	${MKDIR} tests/output
	${DESTBINDIR}/${A} direct tests/input/mCrea.mrna tests/input/hCreaGeno.nib \
		tests/input/mCreaGeno.nib > tests/output/testNib
	${DESTBINDIR}/${A} direct tests/input/mCrea.mrna tests/input/creaGeno.2bit \
		>tests/output/testTwoBit
	cd tests && ./testProtNib output/testProtNib
	cd tests && ./testProtTwoBit output/testProtTwoBit
	cd tests && ./testTransNib output/testTransNib
	cd tests && ./testTransTwoBit output/testTransTwoBit
	cd tests && ./testPcr output/testPcr
	diff -r -x CVS tests/expected tests/output
	rm -r tests/output

clean::
	rm -fr ${O} tests/output
