include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = featureBits.o

featureBits: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/featureBits $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/featureBits${EXE}
