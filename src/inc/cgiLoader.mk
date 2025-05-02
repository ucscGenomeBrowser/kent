########################################################################
# make rules for programs that CGI loaders.
#
# These are all userApps as well and this includes userApps.mk
#
#	kentSrc = ../..
#	A = aveCols
#	include ${kentSrc}/inc/cgiLoader.mk
#
# For multiple programs use USER_APP_RECURSE_TARGETS from userApps.mk, only
# including this file instead of userApps.mk
#
########################################################################

include ${kentSrc}/inc/userApp.mk
include ${kentSrc}/inc/cgiVars.mk


# this target uses CGI_BIN_DEST set in cgiVars.mk to do any of the CGI targers
default cgi alpha beta:: compile ${SQL_FILES:%=%_sql_install}
	chmod a+rx ${A}${EXE}
	${MKDIR} ${CGI_LOADER_DEST}
	chmod a+rx ${A}${EXE}
	mv -f ${A}${EXE} ${CGI_LOADER_DEST}/

%_sql_install:
	@${MKDIR} ${CGI_LOADER_DEST}
	cp -fp ${CPREMDESTOPT} $* ${CGI_LOADER_DEST}/

