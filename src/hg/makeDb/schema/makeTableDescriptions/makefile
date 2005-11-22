include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = makeTableDescriptions.o

makeTableDescriptions: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/makeTableDescriptions $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/makeTableDescriptions

clean:
	rm -f $O
