include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtPretty.o

axtPretty: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtPretty $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtPretty${EXE}

clean::
	rm -f ${O}
