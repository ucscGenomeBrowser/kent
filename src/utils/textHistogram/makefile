include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = textHistogram.o

textHistogram: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/textHistogram $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/textHistogram${EXE}

clean:
	rm -f ${O}
