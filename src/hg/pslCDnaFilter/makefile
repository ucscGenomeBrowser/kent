include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

H = cDnaAligns.h cDnaStats.h cDnaReader.h overlapFilter.h polyASize.h hapRegions.h usage.msg algo.msg
O = cDnaAligns.o cDnaStats.o cDnaReader.o overlapFilter.o polyASize.o hapRegions.o pslCDnaFilter.o
B =  ${DESTDIR}${BINDIR}/pslCDnaFilter

$B: pslCDnaFilter
	@${MKDIR} `dirname $B`
	cp -f pslCDnaFilter $B

pslCDnaFilter: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslCDnaFilter $O ${MYLIBS} ${MYSQLLIBS} $L

pslCDnaFilter.o: pslCDnaFilter.c $H
cDnaAligns.o: cDnaAligns.c $H
overlapFilter.o: overlapFilter.c $H
polyASizes.o: polyASizes.c polyASizes.h

# create msg from text
%.msg: %.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O pslCDnaFilter usage.msg algo.msg
	cd tests && ${MAKE} clean

savebak:
	savebak pslCDnaFilter *.c *.h *.as usage.txt makefile tests/makefile tests/input

