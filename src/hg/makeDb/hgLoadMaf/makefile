include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

AO = hgLoadMaf.o
A = hgLoadMaf
BO = hgLoadMafSummary.o
B = hgLoadMafSummary

both:	hgLoadMaf hgLoadMafSummary

hgLoadMafSummary: ${BO} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${B} ${BO} ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadMafSummary${EXE}

hgLoadMaf: ${AO} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} ${AO} ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadMaf${EXE}

compile:	${AO} ${BO} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A} ${AO} ${MYLIBS} $L
	${CC} ${COPT} ${CFLAGS} -o ${B} ${BO} ${MYLIBS} $L

cgi:: compile
	chmod a+rx $A${EXE}
	@if [ ! -d "${CGI_BIN}-${USER}/loader" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/loader; \
	fi
	rm -f ${CGI_BIN}-${USER}/loader/$A
	mv $A${EXE} ${CGI_BIN}-${USER}/loader/$A

alpha:: strip
	@if [ ! -d "${CGI_BIN}/loader" ]; then \
		${MKDIR} ${CGI_BIN}/loader; \
	fi
	rm -f ${CGI_BIN}/loader/$A
	mv $A${EXE} ${CGI_BIN}/loader/$A

beta:: strip
	@if [ ! -d "${CGI_BIN}-beta/loader" ]; then \
		${MKDIR} ${CGI_BIN}-beta/loader; \
	fi
	rm -f ${CGI_BIN}-beta/loader/$A
	mv $A${EXE} ${CGI_BIN}-beta/loader/$A

strip::  compile
	${STRIP} $A${EXE}
	chmod g+w $A${EXE}
	chmod a+rx $A${EXE}
	${STRIP} $B${EXE}
	chmod g+w $B${EXE}
	chmod a+rx $B${EXE}

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}/loader" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}/loader"; \
	fi
	rm -f ${DESTDIR}${CGI_BIN}/loader/$A
	mv $A${EXE} ${DESTDIR}${CGI_BIN}/loader/$A

debug:: $O
	${CC} ${AO} ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	${CC} ${BO} ${MYLIBS} ${L}
	mv ${AOUT} $B${EXE}

clean::
	rm -f ${AO} ${BO} $A${EXE} $B${EXE} tests/bed.tab

test:
	cd tests && ./RunTest.sh

test_verbose:
	cd tests && ./RunTest.sh -verbose

