kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkweb.a 

O = combineFiles.o

combineFiles: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/combineFiles $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/combineFiles${EXE}

clean:
	rm -f $O
