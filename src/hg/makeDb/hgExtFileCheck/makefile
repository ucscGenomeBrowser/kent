include ../../../inc/common.mk


L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgExtFileCheck.o

hgExtFileCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgExtFileCheck $O $(MYLIBS) $L

clean::
	rm -f ${O}
