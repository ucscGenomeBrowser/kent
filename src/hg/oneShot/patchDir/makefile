kentSrc = ../../..
include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patchDir.o

patchDir: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/patchDir $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/patchDir${EXE}

clean::
	rm -f ${O}
