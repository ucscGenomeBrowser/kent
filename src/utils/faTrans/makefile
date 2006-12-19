include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faTrans.o

faTrans: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faTrans $O $(MYLIBS) $L
