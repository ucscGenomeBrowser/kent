include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = embossToPsl.o
BINDIR = $(HOME)/bin/$(MACHTYPE)

embossToPsl: $O $(MYLIBS)
	${CC} -ggdb -o embossToPsl $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f embossToPsl ${BINDIR}
	${STRIP} ${BINDIR}/embossToPsl${EXE}

test:: embossToPsl
	cd tests && ${MAKE} test
clean::
	rm -f ${O} embossToPsl
	cd tests && ${MAKE} clean
