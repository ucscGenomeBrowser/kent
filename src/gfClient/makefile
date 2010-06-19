include ../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o
X = gfClient

gfClient: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$X $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$X${EXE}

clean::
	rm -f ${O}
