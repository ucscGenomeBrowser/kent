include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedThickToCds.o

bedThickToCds: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedThickToCds $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedThickToCds${EXE}

test:	bedThickToCds
	bedThickToCds in.bed out.cds
	diff expected.cds out.cds

clean:
	rm -f ${O}
