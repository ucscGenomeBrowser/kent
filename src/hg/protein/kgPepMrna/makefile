
#	$Id: makefile,v 1.2 2006/06/14 16:54:46 hiram Exp $

include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = kgPepMrna

O = $A.o

my: compile
	mv $A${EXE} ${HOME}/bin/${MACHTYPE}/$A

mgsc: compile
	mv $A${EXE} /usr/local/apache/cgi-bin-mgsc/$A

alpha: compile
	mv $A${EXE} /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A${EXE} /usr/local/apache/cgi-bin-beta/$A


compile: $O 
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv a.out $A${EXE}
#	${STRIP} $A${EXE}
	chmod a+rx $A${EXE}

lib:
	cd ../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f $A${EXE} $A.o
