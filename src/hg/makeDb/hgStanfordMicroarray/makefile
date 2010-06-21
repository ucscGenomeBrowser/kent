include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgStanfordMicroarray.o

hgStanfordMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgStanfordMicroarray $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgStanfordMicroarray${EXE}

clean::
	rm -f ${O}
