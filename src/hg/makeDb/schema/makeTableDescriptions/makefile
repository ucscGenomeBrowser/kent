include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = makeTableDescriptions.o

makeTableDescriptions: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/makeTableDescriptions $O ${MYLIBS} $L
	strip ${BINDIR}/makeTableDescriptions

clean:
	rm -f $O
