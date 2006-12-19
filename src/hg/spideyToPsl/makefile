include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}
L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spideyToPsl.o

spideyToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o spideyToPsl $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f spideyToPsl ${BINDIR}
	${STRIP} ${BINDIR}/spideyToPsl${EXE}

test::
	cd tests && ${MAKE} test
clean::
	rm -f ${O} spideyToPsl ${BINDIR}/spideyToPsl
	cd tests && ${MAKE} clean
