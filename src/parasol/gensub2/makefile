kentSrc = ../..
include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gensub2.o

gensub2: $O $(MYLIBS)
	${MKDIR} ../bin
	${CC} ${COPT} ${CFLAGS} -o ../bin/gensub2 $O $(MYLIBS) $L
	${STRIP} ../bin/gensub2${EXE}

clean::
	rm -f ${O}
