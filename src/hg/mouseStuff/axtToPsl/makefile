include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtToPsl.o

axtToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtToPsl $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtToPsl${EXE}

test:
	(cd tests && ${MAKE} test)
