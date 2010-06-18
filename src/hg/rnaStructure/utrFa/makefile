include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = utrFa.o

utrFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/utrFa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/utrFa${EXE}

clean::
	rm -f ${O}
