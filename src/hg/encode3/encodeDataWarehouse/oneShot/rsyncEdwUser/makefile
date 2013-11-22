kentSrc = ../../../..
include $(kentSrc)/inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = $(kentSrc)/lib/${MACHTYPE}
MYLIBS =  ../lib/$(MACHTYPE)/edwlib.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = rsyncEdwUser

O = $A.o 

compile: $O $(MYLIBS) 
	${CC} ${COPT} -o rsyncEdwUser $O ${MYLIBS} $L
#	@./rsyncEdwUser

clean::
	rm -f rsyncEdwUser ${O}
