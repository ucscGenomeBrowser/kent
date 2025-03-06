kentSrc = ../../..
# recursive make uglyness (see userArp.mk)
ifeq (${PROG},)
PROGS = hgLoadWiggle hgWiggle wigEncode

default:: ${PROGS:%=%_default}
%_default:
	${MAKE} default PROG=$*
compile:: ${PROGS:%=%_compile}
%_compile:
	${MAKE} compile PROG=$*
clean:: ${PROGS:%=%_clean}
%_clean::
	${MAKE} clean PROG=$*
install: ${PROGS:%=%_}
%_install:
	${MAKE} install PROG=$*

cgi:: cgi_any
alpha:: cgi_any
beta:: cgi_any
cgi_any:: ${PROGS:%=%_cgi_any}
%_cgi_any:
	${MAKE} cgi_any PROG=$*

test::
	${MAKE} test PROG=testing
else
A = ${PROG}
include ${kentSrc}/inc/cgiLoader.mk

perl_progs = fixStepToBedGraph varStepToBedGraph
default:: ${perl_progs:%=${DESTDIR}${BINDIR}/%.pl}

${DESTDIR}${BINDIR}/%.pl: %.pl
	${MKDIR} $(dir $@)
	cp -f $< $@

# tests are currently broken
# test::
#	cd tests && ./RunTest.sh

clean::
	rm -f test_data/chr1_0.wib test_data/chr1_0.wig test_data/chr1_1.wib \
	test_data/chr1_1.wig test_data/chr1_2.wib test_data/chr1_2.wig \
	test_data/chr1_begin.ascii test_data/chr1_end.ascii \
	test_data/chr1_middle.ascii test_data/chrM.ascii test_data/chrM.wib \
	test_data/chrM.wig test_data/wiggle.tab

endif
