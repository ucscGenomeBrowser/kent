include ../../inc/common.mk


L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = maskOutFa.o

maskOutFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/maskOutFa $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/maskOutFa${EXE}
