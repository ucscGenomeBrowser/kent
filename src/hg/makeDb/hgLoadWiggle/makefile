kentSrc = ../../..
ifeq (${PROG},)
PROGS = hgLoadWiggle hgWiggle wigEncode
include ${kentSrc}/inc/userAppMulti.mk
else
A = ${PROG}
include ${kentSrc}/inc/cgiLoader.mk

perl_progs = fixStepToBedGraph varStepToBedGraph
default:: ${perl_progs:%=${DESTDIR}${BINDIR}/%.pl}

${DESTDIR}${BINDIR}/%.pl: %.pl
	${MKDIR} $(dir $@)
	cp -f $< $@

# FIXME: tests are currently broken
# test::
#	cd tests && ./RunTest.sh

clean::
	rm -f test_data/chr1_0.wib test_data/chr1_0.wig test_data/chr1_1.wib \
	test_data/chr1_1.wig test_data/chr1_2.wib test_data/chr1_2.wig \
	test_data/chr1_begin.ascii test_data/chr1_end.ascii \
	test_data/chr1_middle.ascii test_data/chrM.ascii test_data/chrM.wib \
	test_data/chrM.wig test_data/wiggle.tab

endif
