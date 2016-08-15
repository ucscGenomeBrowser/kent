kentSrc = ../../..
include ../../../inc/common.mk

L += $(EXPATLIB) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = t.o

t: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o t $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/autoXml${EXE}

clean::
	rm -f ${O}
