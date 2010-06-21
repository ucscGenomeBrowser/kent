include ../../../inc/common.mk

L += -lm $(MYSQLLIBS) 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pepPredToFa.o

pepPredToFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pepPredToFa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/pepPredToFa${EXE}

clean::
	rm -f ${O}
