include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = weedTab.o

weedTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/weedTab $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/weedTab${EXE}
