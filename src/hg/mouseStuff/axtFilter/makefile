include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtFilter.o

axtFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtFilter $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtFilter${EXE}

clean::
	rm -f ${O}
