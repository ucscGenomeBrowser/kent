include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslCheck.o

pslCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslCheck $O $(MYLIBS) $L
