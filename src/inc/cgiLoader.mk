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
include ${kentSrc}/inc/cgiVars.mk


# this target uses CGI_BIN_DEST set in cgiVars.mk to do any of the CGI targers
cgi_any:: compile ${SQL_FILES:%=%_sql_install}
	chmod a+rx ${A}${EXE}
	${MKDIR} ${CGI_LOADER_DEST}
	chmod a+rx ${A}${EXE}
	mv -f ${A}${EXE} ${CGI_LOADER_DEST}/

%_sql_install:
	@${MKDIR} ${CGI_LOADER_DEST}
	cp -fp --remove-destination $* ${CGI_LOADER_DEST}/

cgi:: cgi_any
alpha:: cgi_any
beta:: cgi_any
