.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgc

O = $A.o

my: compile
	mv $A /usr/local/apache/cgi-bin-$(USER)/$A

mgsc: compile
	mv $A /usr/local/apache/cgi-bin-mgsc/$A

alpha: compile
	mv $A /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A /usr/local/apache/cgi-bin-beta/$A

compile: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A
#	strip $A
	chmod a+rx $A

lib:
	cd ../../lib; make

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
