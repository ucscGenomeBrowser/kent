include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = pslHisto.o
B =  ${HOME}/bin/${MACHTYPE}/pslHisto

$B: pslHisto
	@${MKDIR} `dirname $B`
	cp -f pslHisto $B

pslHisto: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslHisto $O ${MYLIBS} ${MYSQLLIBS} $L

pslHisto.o: pslHisto.c $H

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B pslHisto
	cd tests && ${MAKE} clean
