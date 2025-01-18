########################################################################
# make rules for programs that CGI loaders.
#
# These are all userApps as well and this includes userApps.mk
#
#	kentSrc = ../..
#	A = aveCols
#	include ${kentSrc}/inc/cgiLoader.mk
#
########################################################################

include ${kentSrc}/inc/userApp.mk

cgi:: compile
	chmod a+rx ${A:%=%${EXE}}
	${MKDIR} ${CGI_BIN}-${USER}/loader; \
	mv -f ${A:%=%${EXE}} ${CGI_BIN}-${USER}/loader/
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}-${USER}/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}-${USER}/loader/$$B ; \
	done

alpha:: strip
	${MKDIR} ${CGI_BIN}/loader; \
	mv -f ${A:%=%${EXE}} ${CGI_BIN}/loader/
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}/loader/$$B ; \
	done

beta:: strip
	${MKDIR} ${CGI_BIN}-beta/loader; \
	mv -f ${A:%=%${EXE}} ${CGI_BIN}-beta/loader/
	for F in ${SQL_FILES}; do \
	    B=`basename $$F` ; \
	    rm -f ${DESTDIR}${CGI_BIN}-beta/loader/$$B ; \
	    cp -p $$F ${DESTDIR}${CGI_BIN}-beta/loader/$$B ; \
	done


