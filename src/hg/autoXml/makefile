include ../../inc/common.mk


MYLIBDIR = ../../lib/$(MACHTYPE)
#MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = autoXml.o

autoXml: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/autoXml $O $(MYLIBS)
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/autoXml${EXE}
