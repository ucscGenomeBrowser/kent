kentSrc = ../..
include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = vulgarToPsl
O = ${A}.o

compile: $(O) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/${A}${EXE}

clean:
	rm -f $(O)
