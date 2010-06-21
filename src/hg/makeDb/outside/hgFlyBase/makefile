include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFlyBase.o

hgFlyBase: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgFlyBase $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/hgFlyBase${EXE}

clean::
	rm -f ${O}
