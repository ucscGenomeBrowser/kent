include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = xmlCat.o

xmlCat: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/xmlCat $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/xmlCat

test:: 
	doTest

clean:
	rm -f $O
