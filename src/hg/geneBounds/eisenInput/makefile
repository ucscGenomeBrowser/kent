include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = eisenInput.o

eisenInput: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/eisenInput $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/eisenInput${EXE}
