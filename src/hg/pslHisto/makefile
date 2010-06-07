include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslHisto.o

${BINDIR}/pslHisto: $O ${MYLIBS}
	@${MKDIR} ${BINDIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $O ${MYLIBS} ${MYSQLLIBS} $L

pslHisto.o: pslHisto.c $H

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B pslHisto
	cd tests && ${MAKE} clean
