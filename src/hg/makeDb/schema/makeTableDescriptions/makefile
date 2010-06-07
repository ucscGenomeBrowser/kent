include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = makeTableDescriptions.o

makeTableDescriptions: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/makeTableDescriptions $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/makeTableDescriptions${EXE}

clean:
	rm -f $O
