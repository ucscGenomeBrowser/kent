#	Common set of build rules for CGI binaries

my:: compile
	chmod a+rx $A
	mv $A ${CGI_BIN}-${USER}/$A

alpha:: strip
	mv $A ${CGI_BIN}/$A

beta:: strip
	mv $A ${CGI_BIN}-beta/$A

strip::  compile
	strip $A
	chmod g+w $A
	chmod a+rx $A

install::  strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A ${DESTDIR}${CGI_BIN}/$A

debug:: $O
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A


lib::
	cd ../../lib; make

clean::
	rm -f $O $A

tags::
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

