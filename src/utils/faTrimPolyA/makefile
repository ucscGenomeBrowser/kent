include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faTrimPolyA.o

faTrimPolyA: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faTrimPolyA $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faTrimPolyA${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	rm -f ${O}
