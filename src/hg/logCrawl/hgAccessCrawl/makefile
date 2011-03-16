include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgAccessCrawl.o

hgAccessCrawl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgAccessCrawl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgAccessCrawl${EXE}

clean::
	rm -f ${O}
