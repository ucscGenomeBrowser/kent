.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgText.o

hgText: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out hgText2
	strip hgText2
	chmod a+rx hgText2
	mv hgText2 $(HGCGI)

lib:
	cd ../../lib; make

tags: 
		ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
