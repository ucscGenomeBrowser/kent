include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpToGl.o

agpToGl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/agpToGl $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/agpToGl
