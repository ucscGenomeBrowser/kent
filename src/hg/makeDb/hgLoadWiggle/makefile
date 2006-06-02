include ../../../inc/common.mk
HG_WARN=${HG_WARN_ERR}

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadWiggle.o

A = hgLoadWiggle

all::	${A} hgWiggle wigEncode cgi

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

cgi:: compile
	@if [ ! -d "${CGI_BIN}-${USER}/loader" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/loader; \
	fi
	mv $A${EXE} ${CGI_BIN}-${USER}/loader/$A
	mv wigEncode ${CGI_BIN}-${USER}/loader/wigEncode

alpha:: strip
	@if [ ! -d "${CGI_BIN}/loader" ]; then \
		${MKDIR} ${CGI_BIN}/loader; \
	fi
	mv $A${EXE} ${CGI_BIN}/loader/$A
	mv wigEncode ${CGI_BIN}/loader/wigEncode

beta:: strip
	@if [ ! -d "${CGI_BIN}-beta/loader" ]; then \
		${MKDIR} ${CGI_BIN}-beta/loader; \
	fi
	mv $A${EXE} ${CGI_BIN}-beta/loader/$A
	mv wigEncode ${CGI_BIN}-beta/loader/wigEncode

strip::  compile
	${STRIP} $A${EXE}
	${STRIP} wigEncode
	chmod g+w $A${EXE} wigEncode
	chmod a+rx $A${EXE} wigEncode

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}/loader" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}/loader"; \
	fi
	mv $A${EXE} ${DESTDIR}${CGI_BIN}/loader/$A
	mv wigEncode ${DESTDIR}${CGI_BIN}/loader/wigEncode

debug:: $O
	${CC} -o $A${EXE} $O ${MYLIBS} ${L}
