include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = venn.o

venn: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/venn $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/venn${EXE}
