kentSrc = ../..
include ../../inc/common.mk
L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = est2genomeToPsl.o

est2genomeToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o est2genomeToPsl $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f est2genomeToPsl ${BINDIR}
	${STRIP} ${BINDIR}/est2genomeToPsl${EXE}

test::
	cd tests && ${MAKE} test

clean::
	rm -f ${O} est2genomeToPsl ${BINDIR}/est2genomeToPsl
	cd tests && ${MAKE} clean
