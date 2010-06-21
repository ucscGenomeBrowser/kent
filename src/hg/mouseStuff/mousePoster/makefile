include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mousePoster.o

mousePoster: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mousePoster $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mousePoster${EXE}

clean::
	rm -f ${O}
