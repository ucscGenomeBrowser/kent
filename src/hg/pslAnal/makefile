include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = pslAnal

O = $A.o

compile: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A
	strip $A
	chmod a+rx $A
	cp $A /cluster/home/booch/bin/i386

debug: $O
	gcc $O $(MYLIBS) $(L) 
	mv a.out $A

lib:
	cd ../../lib; make

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
