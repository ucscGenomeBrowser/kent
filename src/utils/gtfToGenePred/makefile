include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 
HG_INC=-I../../inc -I../../hg/inc

O = gtfToGenePred.o

gtfToGenePred: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(BINDIR)/gtfToGenePred${EXE} $O $(MYLIBS) $L

test:
	(cd tests && ${MAKE} test)

clean:
	rm $O
	(cd tests && ${MAKE} clean)
