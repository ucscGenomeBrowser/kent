include ../../inc/common.mk
COPT += ${HG_WERR_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


H = cDnaAligns.h overlapFilter.h polyASizes.h
O = pslCDnaFilter.o cDnaAligns.o overlapFilter.o polyASizes.o
B =  ${HOME}/bin/${MACHTYPE}/pslCDnaFilter

$B: pslCDnaFilter
	@${MKDIR} `dirname $B`
	cp -f pslCDnaFilter $B

pslCDnaFilter: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslCDnaFilter $O ${MYLIBS} ${MYSQLLIBS} $L

pslCDnaFilter.o: pslCDnaFilter.c $H
cDnaAligns.o: cDnaAligns.c $H
overlapFilter.o: overlapFilter.c $H
polyASizes.o: polyASizes.c polyASizes.h

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B
	#cd tests && ${MAKE} clean

savebak:
	savebak pslCDnaFilter *.c *.h *.as makefile tests/input debug/bin
