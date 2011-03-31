include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = weedTab.o

weedTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/weedTab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/weedTab${EXE}

clean::
	rm -f ${O}
