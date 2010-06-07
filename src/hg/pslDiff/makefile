include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = pslSets.o pslDiff.o
# pslDiff.o
B =  ${BINDIR}/pslDiff

$B: pslDiff
	@${MKDIR} `dirname $B`
	cp -f pslDiff $B

pslDiff: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslDiff $O ${MYLIBS} ${MYSQLLIBS} $L

pslDiff.o: pslDiff.c pslSets.h

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B pslDiff
	cd tests && ${MAKE} clean

savebak:
	savebak pslDiff makefile *.c
