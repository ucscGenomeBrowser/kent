kentSrc = ../../..
A = mafRc
include $(kentSrc)/inc/userApp.mk


test:: test1 testHg16

test1: mkdirs
	${BINDIR}/mafRc ../mafSplit/test/test1.maf output/$@-rc.maf
	diff expected/$@-rc.maf output/$@-rc.maf

testHg16: mkdirs
	${BINDIR}/mafRc ../mafFrags/expected/hg16.maf output/$@-rc.maf
	diff expected/$@-rc.maf output/$@-rc.maf

mkdirs:
	mkdir -p output

clean::
	rm -rf output
