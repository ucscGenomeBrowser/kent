include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRatioMicroarray.o

hgRatioMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgRatioMicroarray $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgRatioMicroarray${EXE}

test:
	hgRatioMicroarray gnfHumanU95All gnfHumanU95AllRatio -clump=gnfClump.ra -noLoad -limit=100 -tab=output
	diff -x CVS -r expected output
	rm -r output
