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

