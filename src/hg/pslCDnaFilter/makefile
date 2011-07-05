include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

.SECONDARY:

H = cDnaAligns.h cDnaStats.h cDnaReader.h overlapFilter.h globalNearBestFilter.h localNearBestFilter.h polyASize.h hapRegions.h usage.msg algo.msg
O = cDnaAligns.o cDnaStats.o cDnaReader.o overlapFilter.o globalNearBestFilter.o localNearBestFilter.o polyASize.o hapRegions.o pslCDnaFilter.o
A =  pslCDnaFilter

$A: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}
	@rm -f usage.msg algo.msg

pslCDnaFilter.o: pslCDnaFilter.c $H
cDnaAligns.o: cDnaAligns.c $H
cDnaStats.o: cDnaStats.c $H
overlapFilter.o: overlapFilter.c $H
hapRegions.o: hapRegions.c $H
polyASizes.o: polyASizes.c polyASizes.h

# create msg from text
%.msg: %.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test: compile
	cd tests && ${MAKE} test
	rm -f ${A}${EXE} 

clean:
	rm -f $O pslCDnaFilter usage.msg algo.msg
	cd tests && ${MAKE} clean

savebak:
	savebak pslCDnaFilter *.c *.h *.as usage.txt makefile tests/makefile tests/input

