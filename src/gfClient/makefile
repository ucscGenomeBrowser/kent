include ../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o
X = gfClient

gfClient: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/$X $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/$X${EXE}

clean::
	rm -f ${O}
