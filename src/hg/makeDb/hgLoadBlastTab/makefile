include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadBlastTab.o

hgLoadBlastTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadBlastTab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadBlastTab${EXE}

clean::
	rm -f ${O}
