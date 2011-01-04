#	Common set of build rules for CGI binaries

my:: compile
	chmod a+rx $A${EXE}
	mv $A${EXE} ${CGI_BIN}-${USER}/$A

alpha:: strip
	mv $A${EXE} ${CGI_BIN}/$A

beta:: strip
	mv $A${EXE} ${CGI_BIN}-beta/$A

# don't actually strip so we can get stack traces
strip::  compile
	chmod g+w $A${EXE}
	chmod a+rx $A${EXE}

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	rm -f ${DESTDIR}${CGI_BIN}/$A
	mv $A${EXE} ${DESTDIR}${CGI_BIN}/$A

debug:: $O
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

lib::
	cd ../../lib; make

clean::
	rm -f $O $A${EXE}

tags::
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
