include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gensub2.o

gensub2: $O $(MYLIBS)
	gcc -ggdb -o ../bin/gensub2 $O $(MYLIBS) $L
	strip ../bin/gensub2
