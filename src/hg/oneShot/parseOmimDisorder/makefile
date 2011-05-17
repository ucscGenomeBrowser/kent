include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = parseOmimDisorder.o

parseOmimDisorder: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/parseOmimDisorder $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/parseOmimDisorder${EXE}

clean::
	rm -f ${O}
