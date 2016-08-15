kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastzScripts.o

blastzScripts: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blastzScripts $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/blastzScripts${EXE}

clean::
	rm -f ${O}
