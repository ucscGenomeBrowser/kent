.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = checkDbSync.o

checkDbSync: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/checkDbSync $O $(MYLIBS) $(L)
	strip $(HOME)/bin/$(MACHTYPE)/checkDbSync

