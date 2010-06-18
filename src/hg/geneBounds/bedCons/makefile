include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedCons.o

bedCons: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedCons $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/bedCons${EXE}

clean::
	rm -f ${O}
