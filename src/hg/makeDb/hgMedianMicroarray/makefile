include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgMedianMicroarray.o

hgMedianMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgMedianMicroarray $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgMedianMicroarray${EXE}

test:
	${MKDIR} output
	hgMedianMicroarray ce1 kimLifeCycle kimWormLifeCycle kimMed.ra kimLifeCycleMedian kimWormLifeCycleMedian -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr ${O} output
