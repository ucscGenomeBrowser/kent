include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = regionPicker.o

regionPicker: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/regionPicker $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/regionPicker

tags:
	ctags *.c $(HOME)/src/lib/*.c $(HOME)/src/inc/*.h
