include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = embossToPsl.o

embossToPsl: $O $(MYLIBS)
	${CC} -o embossToPsl $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f embossToPsl ${BINDIR}
	${STRIP} ${BINDIR}/embossToPsl${EXE}

test:: embossToPsl
	cd tests && ${MAKE} test
clean::
	rm -f ${O} embossToPsl
	cd tests && ${MAKE} clean
