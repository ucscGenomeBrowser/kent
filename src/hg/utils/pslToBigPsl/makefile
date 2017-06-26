COPT=-ggdb
kentSrc = ../../..
A = pslToBigPsl
include $(kentSrc)/inc/userApp.mk

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

debug:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

clean::
	rm -f $O $A${EXE} tests/bed.tab

