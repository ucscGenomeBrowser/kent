#	Common set of build rules for CGI binaries

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my:: compile
	chmod a+rx $A${EXE}
	rm -f ${CGI_BIN_USER}/$A
	mv $A${EXE} ${CGI_BIN_USER}/$A

alpha:: strip
	rm -f ${CGI_BIN}/$A
	mv $A${EXE} ${CGI_BIN}/$A

beta:: strip
	rm -f ${CGI_BIN_BETA}/$A
	mv $A${EXE} ${CGI_BIN_BETA}/$A

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
	rm -f $A${EXE}
	mv ${AOUT} $A${EXE}

lib::
	cd ../../lib; make

clean::
	rm -f $O $A${EXE}

tags::
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
