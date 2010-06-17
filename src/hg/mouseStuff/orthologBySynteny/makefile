include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = orthologBySynteny.o

orthologBySynteny: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/orthologBySynteny $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/orthologBySynteny${EXE}

clean::
	rm -f ${O}
