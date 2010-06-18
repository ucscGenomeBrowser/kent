include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = assessLibs.o 

assessLibs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/assessLibs $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/assessLibs${EXE}

clean::
	rm -f ${O}
