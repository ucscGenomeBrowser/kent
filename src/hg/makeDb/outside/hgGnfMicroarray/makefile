include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGnfMicroarray.o

hgGnfMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgGnfMicroarray $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgGnfMicroarray${EXE}

test:
	${MKDIR} output
	hgGnfMicroarray -tab=output -noLoad -limit=100 gnfHumanU95Exps gnfHumanU95All /projects/compbio/data/microarray/affyGnfHuman/data_public_U95
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr ${O} output
