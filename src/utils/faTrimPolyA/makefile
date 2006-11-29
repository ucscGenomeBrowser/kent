include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faTrimPolyA.o

faTrimPolyA: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faTrimPolyA $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faTrimPolyA${EXE}

test:
	(cd tests && ${MAKE} test)
