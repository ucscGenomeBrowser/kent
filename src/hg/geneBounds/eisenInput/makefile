include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = eisenInput.o

eisenInput: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/eisenInput $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/eisenInput${EXE}

clean::
	rm -f ${O}
