include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToControlTable.o

iriToControlTable: gbLib $O $(MYLIBS)
	cd ../lib && ${MAKE}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/iriToControlTable $O ../lib/improbRunInfo.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/iriToControlTable${EXE}

gbLib::
	( cd ../lib && ${MAKE} )

clean::
	rm -f ${O}
