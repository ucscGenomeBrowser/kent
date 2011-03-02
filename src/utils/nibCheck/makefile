include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = nibCheck.o

nibCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/nibCheck $O $(MYLIBS) $L
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) ${L} -lm
	mv ${AOUT} nibCheck
	chmod a+rx nibCheck

clean::
	rm -f ${O}
