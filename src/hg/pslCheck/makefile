include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
L += $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslCheck.o

pslCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslCheck $O $(MYLIBS) $L

clean::
	rm -f ${O}
