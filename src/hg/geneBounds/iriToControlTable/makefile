include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToControlTable.o ../lib/improbRunInfo.o

iriToControlTable: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/iriToControlTable $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/iriToControlTable
