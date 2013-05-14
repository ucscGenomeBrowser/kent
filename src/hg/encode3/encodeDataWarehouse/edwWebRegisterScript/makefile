kentSrc = ../../../..
include $(kentSrc)/inc/common.mk

L += -luuid -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../lib/$(MACHTYPE)/edwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = edwWebRegisterScript

O = $A.o 

include $(kentSrc)/inc/cgi_build_rules.mk

compile: $O 
	${CC} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
