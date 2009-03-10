include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faCat.o

faCat: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faCat $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faCat${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	(cd tests && ${MAKE} clean)
