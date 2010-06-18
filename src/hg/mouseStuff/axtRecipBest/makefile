include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtRecipBest.o

axtRecipBest: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtRecipBest $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtRecipBest${EXE}

tags:  
	ctags *.c ../../../lib/*.c ../../../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f ${O}
