include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = spacedToTab.o

spacedToTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/spacedToTab $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/spacedToTab${EXE}
