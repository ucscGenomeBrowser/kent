kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToFa.o

pslToFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o pslToFa $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/pslToFa${EXE}

clean::
	rm -f ${O}
