include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netSyntenic.o

netSyntenic: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netSyntenic $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netSyntenic${EXE}

clean::
	rm -f ${O}
