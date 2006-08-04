include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
#COPT=-ggdb

A = extractPslForLfs

O = $A.o

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
	${STRIP} $A${EXE}
	chmod a+rx $A
	cp $A ${HOME}/bin/${MACHTYPE}

debug: $O
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

lib:
	cd ../../lib && ${MAKE}

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f extractPslForLfs ${O}
