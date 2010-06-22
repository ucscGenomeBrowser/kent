include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
L += $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredToFakePsl.o

genePredToFakePsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/genePredToFakePsl $O $(MYLIBS) $L

clean::
	rm -f ${O}
