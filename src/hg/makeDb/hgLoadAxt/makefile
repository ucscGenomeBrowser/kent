include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadAxt.o

hgLoadAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadAxt $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadAxt${EXE}

clean::
	rm -f ${O}
