include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterRna.o

clusterRna: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/clusterRna $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/clusterRna${EXE}

tags:
	ctags *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h

clean::
	rm -f ${O}
