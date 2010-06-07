include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFilter.o

faFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faFilter $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faFilter${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	(cd tests && ${MAKE} clean)
