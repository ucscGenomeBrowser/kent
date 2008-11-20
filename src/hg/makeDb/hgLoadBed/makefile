include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
SQL_FILES = ../../lib/encode/encodePeak.sql

O = hgLoadBed.o
A = hgLoadBed

hgLoadBed: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadBed${EXE}

compile:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

cgi:: compile
	chmod a+rx $A${EXE}
	@if [ ! -d "${CGI_BIN}-${USER}/loader" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/loader; \
	fi
	rm -f ${CGI_BIN}-${USER}/loader/$A
	mv $A${EXE} ${CGI_BIN}-${USER}/loader/$A
	chmod a+r ${SQL_FILES}
	chmod g+w ${SQL_FILES}
	cp ${SQL_FILES} ${CGI_BIN}-${USER}/loader

alpha:: strip
	@if [ ! -d "${CGI_BIN}/loader" ]; then \
		${MKDIR} ${CGI_BIN}/loader; \
	fi
	rm -f ${CGI_BIN}/loader/$A
	mv $A${EXE} ${CGI_BIN}/loader/$A
	chmod a+r ${SQL_FILES}
	chmod g+w ${SQL_FILES}
	cp ${SQL_FILES} ${CGI_BIN}/loader

beta:: strip
	@if [ ! -d "${CGI_BIN}-beta/loader" ]; then \
		${MKDIR} ${CGI_BIN}-beta/loader; \
	fi
	rm -f ${CGI_BIN}-beta/loader/$A
	mv $A${EXE} ${CGI_BIN}-beta/loader/$A
	chmod a+r ${SQL_FILES}
	chmod g+w ${SQL_FILES}
	cp ${SQL_FILES} ${CGI_BIN}-beta/loader

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
	chmod a+r ${SQL_FILES}
	chmod g+w ${SQL_FILES}
	cp ${SQL_FILES} ${DESTDIR}${CGI_BIN}/loader

debug:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

clean::
	rm -f $O $A${EXE} tests/bed.tab

test:
	cd tests && ./RunTest.sh

test_verbose:
	cd tests && ./RunTest.sh -verbose

