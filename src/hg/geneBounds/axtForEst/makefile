include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtForEst.o

axtForEst: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtForEst $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/axtForEst${EXE}

clean::
	rm -f ${O}
