include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadSqlTab.o

hgLoadSqlTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadSqlTab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadSqlTab${EXE}

clean::
	rm -f ${O}
