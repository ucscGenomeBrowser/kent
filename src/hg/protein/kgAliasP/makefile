
# For performance
#.c.o:
#	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

#For debugging:
.c.o:
	gcc -ggdb -Wformat -Wimplicit  -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = kgAliasP

O = $A.o

my: compile
	mv $A /cluster/home/$(USER)/bin/$(MACHTYPE)

compile: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A
	strip $A
	chmod a+rx $A

lib:
	cd ../../lib; make

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

