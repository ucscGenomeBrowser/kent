include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dnaMotifFind.o 

dnaMotifFind: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/dnaMotifFind $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/dnaMotifFind${EXE}

clean::
	rm -f ${O}
