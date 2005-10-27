include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadWiggle.o

A = hgLoadWiggle

all::	${A} hgWiggle wigEncode

hgLoadWiggle: $O ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}

hgDumpWiggle:	hgDumpWiggle.o ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/hgDumpWiggle hgDumpWiggle.o \
		${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgDumpWiggle${EXE}

hgWiggle:	hgWiggle.o ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/hgWiggle hgWiggle.o \
		${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgWiggle${EXE}

hgWiggle.o:	../../inc/wiggle.h hgWiggle.c

wigEncode:	wigEncode.o ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/wigEncode wigEncode.o ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/wigEncode${EXE}

wigEncode.o:	../../inc/wiggle.h wigEncode.c

compile:	${O} hgWiggle.o wigEncode.o ${MYLIBS}
	${CC} -o ${A} $O ${MYLIBS} $L
	${CC} -o hgWiggle hgWiggle.o ${MYLIBS} $L
	${CC} -o wigEncode wigEncode.o ${MYLIBS} $L

test:
	cd tests && ./RunTest.sh

test_verbose:
	cd tests && ./RunTest.sh -verbose

install::
	rm -f ${SCRIPTS}/chkWiggleTable.sh
	cp -p chkWiggleTable.sh ${SCRIPTS}
	rm -f ${SCRIPTS}/varStepToBedGraph.pl
	cp -p varStepToBedGraph.pl ${SCRIPTS}
	rm -f ${SCRIPTS}/fixStepToBedGraph.pl
	cp -p fixStepToBedGraph.pl ${SCRIPTS}

clean:
	rm -f test_data/chr1_0.wib test_data/chr1_0.wig test_data/chr1_1.wib \
	test_data/chr1_1.wig test_data/chr1_2.wib test_data/chr1_2.wig \
	test_data/chr1_begin.ascii test_data/chr1_end.ascii \
	test_data/chr1_middle.ascii test_data/chrM.ascii test_data/chrM.wib \
	test_data/chrM.wig test_data/wiggle.tab \
	${O} hgDumpWiggle.o hgWiggle.o hgWiggle wigEncode.o wigEncode ${A}
