include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = xmfaToMaf.o

xmfaToMaf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/xmfaToMaf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/xmfaToMaf

tags:  
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h
