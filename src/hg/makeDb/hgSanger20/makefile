include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSanger20.o

hgSanger20: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSanger20 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSanger20${EXE}

clean::
	rm -f ${O}
