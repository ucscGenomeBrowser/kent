include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = xmlCat.o

xmlCat: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/xmlCat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/xmlCat${EXE}

test:: 
	doTest

clean:
	rm -f $O
