include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkSgdSync.o

checkSgdSync: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/checkSgdSync $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/checkSgdSync${EXE}
