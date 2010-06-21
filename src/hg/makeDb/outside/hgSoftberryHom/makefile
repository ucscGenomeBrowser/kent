include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSoftberryHom.o

hgSoftberryHom: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSoftberryHom $O $(MYLIBS) $L

clean::
	rm -f ${O}
