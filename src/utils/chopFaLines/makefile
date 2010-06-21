include ../../inc/common.mk


L +=
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chopFaLines.o

chopFaLines: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chopFaLines $O $(MYLIBS) $L

clean::
	rm -f ${O}
