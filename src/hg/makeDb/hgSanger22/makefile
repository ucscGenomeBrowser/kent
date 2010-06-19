include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSanger22.o 

hgSanger22: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgSanger22 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgSanger22${EXE}

clean::
	rm -f ${O}
