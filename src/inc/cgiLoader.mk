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

alpha:: compile
	${MKDIR} ${CGI_BIN}/loader; \
	${STRIP} ${A:%=%${EXE}}
	cp -f ${A:%=%${EXE}} ${CGI_BIN}/loader/

beta:: compile
	${MKDIR} ${CGI_BIN}-beta/loader; \
	${STRIP} ${A:%=%${EXE}}
	cp -f ${A:%=%${EXE}} ${CGI_BIN}-beta/loader/
