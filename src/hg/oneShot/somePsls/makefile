kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = somePsls.o

somePsls: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/somePsls $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/somePsls${EXE}

clean::
	rm -f ${O}
