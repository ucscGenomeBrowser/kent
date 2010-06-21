include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgMapViaSwissProt.o

hgMapViaSwissProt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgMapViaSwissProt $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgMapViaSwissProt${EXE}

clean::
	rm -f ${O}
