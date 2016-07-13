kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
BINDIR = ${BINDIR}

O = chainOverlap.o

chainOverlap: $O $(MYLIBS)
	$(CC) -o chainOverlap $O $(MYLIBS) $L
	cp -f chainOverlap $(BINDIR)
#	${STRIP} $(BINDIR)/chainOverlap${EXE}

clean::
	rm -f chainOverlap ${O}
