include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}
L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ncbiTabToPsl.o ncbiTabAlign.o
BINDIR = $(HOME)/bin/$(MACHTYPE)

ncbiTabToPsl: $O $(MYLIBS)
	gcc -ggdb -o ncbiTabToPsl $O $(MYLIBS) $L
	mkdir -p ${BINDIR}
	cp -f ncbiTabToPsl ${BINDIR}
	strip ${BINDIR}/ncbiTabToPsl

test::
	cd tests && ${MAKE} test
clean::
	rm -f ${O} ncbiTabToPsl ${BINDIR}/ncbiTabToPsl
	cd tests && ${MAKE} clean
