.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgText.o

hgText: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out hgText
	strip hgText
	chmod a+rx hgText
	mv hgText $(HGCGI)

lib:
	cd ../../lib; make

tags: 
		ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
