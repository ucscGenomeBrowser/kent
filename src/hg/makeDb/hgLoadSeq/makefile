include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadSeq.o 

hgLoadSeq: $O ../../../lib/$(MACHTYPE)/jkweb.a makefile
	gcc ${COPT} ${F64} -o $(HOME)/bin/$(MACHTYPE)/hgLoadSeq $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
