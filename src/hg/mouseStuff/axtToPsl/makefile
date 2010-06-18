include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtToPsl.o

axtToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtToPsl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtToPsl${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	rm -f ${O}
