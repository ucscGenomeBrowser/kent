.c.o:
	gcc -ggdb -O -Wall -I$(KENTROOT)/inc -I$(KENTROOT)/hg/inc -c $*.c
#	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I$(KENTROOT)/inc -I$(KENTROOT)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(KENTROOT)/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

A = dbSnp
O = $(A).o

dbSnp: $(O) #$(MYLIBS)
	gcc  -Wall -o $(A) $(O) $(MYLIBS) $(L)
	cp $(A) $(HOME)/bin/$(MACHTYPE)/$(A)

tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h

clean: 
	rm $(O) $(A)
	rm $(HOME)/bin/$(MACHTYPE)/$(A)
