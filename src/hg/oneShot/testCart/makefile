include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = testCart.o cart.o

testCart: $O $(MYLIBS)
	gcc -ggdb -o testCart $O $(MYLIBS) $L
	strip testCart
	mv testCart /usr/local/apache/cgi-bin/testCart
