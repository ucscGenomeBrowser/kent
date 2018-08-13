kentSrc = ../../../..
include $(kentSrc)/inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../lib/$(MACHTYPE)/cdwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = cdwWebBrowse
B = cdwFlowCharts
O = $A.o $B.o

include $(kentSrc)/inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

$O: tagDescriptions.h cdwHelp.h

cdwHelp.h: cdwHelp.html
	stringify cdwHelp.html > cdwHelp.h

my::
	rm -f ${CGI_BIN}-${USER}/cirm-motd.html
	cp cirm-motd.html ${CGI_BIN}-${USER}
	chmod a+r ${CGI_BIN}-${USER}/cirm-motd.html

alpha::
	rm -f ${CGI_BIN}/cirm-motd.html
	cp cirm-motd.html ${CGI_BIN}
	chmod a+r ${CGI_BIN}/cirm-motd.html

beta::
	rm -f ${CGI_BIN}-beta/cirm-motd.html
	cp cirm-motd.html ${CGI_BIN}-beta
	chmod a+r ${CGI_BIN}-beta/cirm-motd.html

install::
	rm -f ${DESTDIR}${CGI_BIN}/cirm-motd.html
	cp cirm-motd.html ${DESTDIR}${CGI_BIN}
	chmod a+r ${DESTDIR}${CGI_BIN}/cirm-motd.html
