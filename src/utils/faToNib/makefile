include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faToNib.o

faToNib: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faToNib $O $(MYLIBS) $L
