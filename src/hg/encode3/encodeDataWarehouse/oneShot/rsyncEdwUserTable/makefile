kentSrc = ../../../../..
include $(kentSrc)/inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../../lib/$(MACHTYPE)/edwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = rsyncEdwUserTable

O = $A.o 

compile: $O $(MYLIBS) 
	${CC} ${COPT} -o rsyncEdwUserTable $O ${MYLIBS} $L
#	@./rsyncEdwUserTable

clean::
	rm -f rsyncEdwUserTable ${O}
