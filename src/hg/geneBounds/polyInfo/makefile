include ../../../inc/common.mk

L += -lm  $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = polyInfo.o

polyInfo: $O $(MYLIBS) makefile
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/polyInfo $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/polyInfo${EXE}

clean::
	rm -f ${O}
