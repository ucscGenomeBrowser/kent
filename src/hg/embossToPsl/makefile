include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = embossToPsl.o
BINDIR = $(HOME)/bin/$(MACHTYPE)

embossToPsl: $O $(MYLIBS)
	gcc -ggdb -o embossToPsl $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f embossToPsl ${BINDIR}
	strip ${BINDIR}/embossToPsl

test::
	cd tests && ${MAKE} test
clean::
	rm -f ${O} embossToPsl ${BINDIR}/embossToPsl
	cd tests && ${MAKE} clean
