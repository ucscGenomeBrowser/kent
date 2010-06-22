include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = spacedToTab.o

spacedToTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/spacedToTab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/spacedToTab${EXE}

test:
	spacedToTab in.txt out.tab
	diff expected.tab out.tab
	spacedToTab in.txt -sizes=17,21,22 out2.tab
	diff expected2.tab out2.tab
	rm -f out.tab out2.tab

clean::
	rm -f ${O} out.tab out2.tab
