include ../../../inc/common.mk

MYLIBDIR = ../../../lib/$(MACHTYPE)
L += $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadMafFrames.o

hgLoadMafFrames: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadMafFrames $O $(MYLIBS) $L


clean:
	rm -f  *.o
