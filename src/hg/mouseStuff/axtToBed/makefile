include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtToBed.o

axtToBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtToBed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtToBed${EXE}

clean::
	rm -f ${O}
