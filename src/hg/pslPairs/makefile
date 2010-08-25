include ../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pslPairs
O = $A.o

$A: $O 
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

debug: $O
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

lib:
	cd ../../lib && ${MAKE}

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f pslPairs ${O}
