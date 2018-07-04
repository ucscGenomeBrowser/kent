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
	cp cirm-motd.html ${CGI_BIN}/

$O: tagDescriptions.h cdwHelp.h

cdwHelp.h: cdwHelp.html
	stringify cdwHelp.html > cdwHelp.h

