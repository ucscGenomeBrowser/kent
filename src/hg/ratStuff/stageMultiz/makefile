include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = stageMultiz.o

stageMultiz: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/stageMultiz $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/stageMultiz

tags: 
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h
