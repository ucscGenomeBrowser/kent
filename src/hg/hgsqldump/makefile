include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgsqldump

O = hgsqldump.o

hgsqldump: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/$A $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/$A${EXE}

clean::
	rm -f ${O}
