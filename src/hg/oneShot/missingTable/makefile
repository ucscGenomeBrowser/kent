kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = missingTable.o 

missingTable: $(O)
	cc -o missingTable $O $(MYLIBS) $L

clean::
	rm -f ${O}
