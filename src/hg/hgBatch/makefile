include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgBatch.o

A = hgBatch

my: compile
	mv $A /usr/local/apache/cgi-bin-$(USER)/$A

alpha: compile
	mv $A /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A /usr/local/apache/cgi-bin-beta/$A

compile: $O $(MYLIBS)
	gcc -ggdb -o $A $O $(MYLIBS) $L
	strip $A
	chmod a+rx $A

lib:
	cd ../lib; make
	cd ../../lib; make

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
