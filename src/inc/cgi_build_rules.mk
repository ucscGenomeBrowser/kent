#	Common set of build rules for CGI binaries

my:: compile
	chmod a+rx $A${EXE}
	mv $A${EXE} ${CGI_BIN}-${USER}/$A${EXE}

alpha:: strip
	mv $A${EXE} ${CGI_BIN}/$A${EXE}

beta:: strip
	mv $A${EXE} ${CGI_BIN}-beta/$A${EXE}

strip::  compile
	${STRIP} $A${EXE}
	chmod g+w $A${EXE}
	chmod a+rx $A${EXE}

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A${EXE} ${DESTDIR}${CGI_BIN}/$A${EXE}

debug:: $O
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A${EXE}


lib::
	cd ../../lib; make

clean::
	rm -f $O $A${EXE}

tags::
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

