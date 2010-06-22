include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRatioMicroarray.o

hgRatioMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgRatioMicroarray $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgRatioMicroarray${EXE}

test:
	${MKDIR} output
	hgRatioMicroarray gnfHumanU95All gnfHumanU95AllRatio -clump=gnfClump.ra -noLoad -limit=100 -tab=output
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr ${O} output
