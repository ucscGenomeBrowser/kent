include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = eisenInput.o

eisenInput: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/eisenInput $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/eisenInput${EXE}
