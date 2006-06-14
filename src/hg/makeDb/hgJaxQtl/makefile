include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgJaxQtl

O = $A.o

my: compile
	mv $A${EXE} ${HOME}/bin/${MACHTYPE}

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
	${STRIP} $A${EXE}
	chmod a+rx $A${EXE}

lib:
	cd ../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h

clean::
	rm -f $A $A.o $A${EXE}

