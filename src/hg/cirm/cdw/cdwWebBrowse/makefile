kentSrc = ../../../..
include $(kentSrc)/inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../lib/$(MACHTYPE)/cdwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = cdwWebBrowse

O = $A.o 

include $(kentSrc)/inc/cgi_build_rules.mk

compile: $O cdwNavBar.h
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

cdwNavBar.h: cdwNavBar.html
	stringify cdwNavBar.html > cdwNavBar.h

