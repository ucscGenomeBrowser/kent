kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = testXap.o

testXap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o testXap $O $(MYLIBS) $L
	${STRIP} testXap${EXE}

clean::
	rm -f ${O}
