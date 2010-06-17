include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtRescore.o

axtRescore: compile
	${STRIP} ${BINDIR}/axtRescore${EXE}

compile: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtRescore $O $(MYLIBS) $L

clean::
	rm -f ${O}
