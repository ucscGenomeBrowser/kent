include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = expToRna.o

expToRna: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/expToRna $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/expToRna${EXE}

clean::
	rm -f ${O}
