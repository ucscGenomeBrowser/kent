include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = makeOrf2gene.o
A = makeOrf2gene

makeOrf2gene: $O ${MYLIBS}
	${CC} -o ${BINDIR}/$A $O ${MYLIBS} ${L}
	${STRIP} ${BINDIR}/$A${EXE}

clean:
	rm -f ${O}
