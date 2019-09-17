kentSrc = ../../..
include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
SQL_FILES = ../../lib/encode/encodePeak.sql ../../lib/pgSnp.sql ../../lib/bedDetail.sql ../../lib/bigPsl.as ../../lib/barChartBed.sql ../../lib/interact.sql

O = hgLoadBed.o
A = hgLoadBed

hgLoadBed: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:       ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

cgi:: compile
	chmod a+rx $A${EXE}
	@if [ ! -d "${CGI_BIN}-${USER}/loader" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/loader; \
	fi
	rm -f ${CGI_BIN}-${USER}/loader/$A
	mv $A${EXE} ${CGI_BIN}-${USER}/loader/$A
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}-${USER}/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}-${USER}/loader/$$B ; \
	done

alpha:: strip
	@if [ ! -d "${CGI_BIN}/loader" ]; then \
		${MKDIR} ${CGI_BIN}/loader; \
	fi
	rm -f ${CGI_BIN}/loader/$A
	mv $A${EXE} ${CGI_BIN}/loader/$A
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	done

beta:: strip
	@if [ ! -d "${CGI_BIN}-beta/loader" ]; then \
		${MKDIR} ${CGI_BIN}-beta/loader; \
	fi
	rm -f ${CGI_BIN}-beta/loader/$A
	mv $A${EXE} ${CGI_BIN}-beta/loader/$A
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}-beta/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}-beta/loader/$$B ; \
	done

strip::  compile
	${STRIP} $A${EXE}
	chmod g+w $A${EXE}
	chmod a+rx $A${EXE}

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}/loader" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}/loader"; \
	fi
	rm -f ${DESTDIR}${CGI_BIN}/loader/$A
	mv $A${EXE} ${DESTDIR}${CGI_BIN}/loader/$A
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	done

debug:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

clean::
	rm -f $O $A${EXE} tests/bed.tab

test:
	cd tests && ./RunTest.sh

test_verbose:
	cd tests && ./RunTest.sh -verbose
