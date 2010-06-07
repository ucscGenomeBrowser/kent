include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToControlTable.o ../lib/improbRunInfo.o

iriToControlTable: $O $(MYLIBS)
	cd ../lib && ${MAKE}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/iriToControlTable $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/iriToControlTable${EXE}
