include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
X = hgNear

O = $X.o colBlast.o

my: compile
	mv $X /usr/local/apache/cgi-bin-$(USER)/$X

alpha: compile
	mv $X /usr/local/apache/cgi-bin/$X

compile: $O $(MYLIBS)
	gcc -ggdb -o $X $O $(MYLIBS) $L
	strip $X
	chmod a+rx $X
