include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = orthologBySynteny.o

orthologBySynteny: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/orthologBySynteny $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/orthologBySynteny${EXE}
