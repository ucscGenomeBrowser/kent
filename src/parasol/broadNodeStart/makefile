include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadNodeStart.o

broadNodeStart: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/broadNodeStart $O $(MYLIBS) $L
	${STRIP} ../bin/broadNodeStart${EXE}

clean::
	rm -f ${O}
