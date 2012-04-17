include ../../inc/common.mk


L +=
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chopFaLines.o

chopFaLines: $O 
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/chopFaLines $O $(MYLIBS) $L

clean::
	rm -f ${O}
