kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = countClient.o

countClient: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/countClient $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/countClient${EXE}

clean::
	rm -f ${O}
