include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = spacedToTab.o

test:
	spacedToTab in.txt out.tab
	diff expected.tab out.tab

spacedToTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/spacedToTab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/spacedToTab${EXE}
