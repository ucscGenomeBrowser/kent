include ../../inc/common.mk
COPT += ${HG_WERR_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


H = pslTbl.h
O = pslHisto.o pslTbl.o
B =  ${HOME}/bin/${MACHTYPE}/pslHisto

$B: pslHisto
	@${MKDIR} `dirname $B`
	cp -f pslHisto $B

pslHisto: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslHisto $O ${MYLIBS} ${MYSQLLIBS} $L

pslHisto.o: pslHisto.c $H
pslTbl.o: pslTbl.c pslTbl.h

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B pslHisto
	cd tests && ${MAKE} clean

savebak:
	savebak pslHisto *.c *.h makefile tests/makefile tests/input
