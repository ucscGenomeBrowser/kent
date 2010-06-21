include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFindSpec.o

hgFindSpec: compile
	${STRIP} ${BINDIR}/hgFindSpec${EXE}

compile: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgFindSpec $O $(MYLIBS) $L

clean::
	rm -f ${O}
