include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToControlTable.o ../lib/improbRunInfo.o

iriToControlTable: $O $(MYLIBS)
	cd ../lib && ${MAKE}
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/iriToControlTable $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/iriToControlTable${EXE}
