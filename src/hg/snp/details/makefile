include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(KENTROOT)/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

A = dbSnp
O = $(A).o

dbSnp: $(O) #$(MYLIBS)
	gcc  -Wall -o $(A) $(O) $(MYLIBS) $(L)
	cp $(A) $(HOME)/bin/$(MACHTYPE)/$(A)

tags:
	ctags *.c *.h ../../lib/*.c ../../inc/*.h

clean: 
	rm $(O) $(A)
	rm $(HOME)/bin/$(MACHTYPE)/$(A)
