include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkweb.a 

O = combineFiles.o

combineFiles: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/combineFiles $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/combineFiles

clean:
	rm -f $O
