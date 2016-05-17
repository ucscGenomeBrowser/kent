kentSrc = ../../../..
include $(kentSrc)/inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../lib/$(MACHTYPE)/cdwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = cdwGetFile

O = $A.o 

include $(kentSrc)/inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
