include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFilter.o

faFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faFilter $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faFilter${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	(cd tests && ${MAKE} clean)
