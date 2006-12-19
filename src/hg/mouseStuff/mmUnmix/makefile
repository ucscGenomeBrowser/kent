include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mmUnmix.o

mmUnmix: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mmUnmix $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mmUnmix${EXE}
