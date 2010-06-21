include ../../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdGfp.o

hgSgdGfp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSgdGfp $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSgdGfp${EXE}

clean::
	rm -f ${O}
