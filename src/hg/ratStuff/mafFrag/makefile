include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFrag.o

mafFrag: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafFrag $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mafFrag${EXE}

test:
	mafFrag hg16 humorMm3Rn3 chr2 69065807 69073183 - test.maf

clean::
	rm -f ${O}
