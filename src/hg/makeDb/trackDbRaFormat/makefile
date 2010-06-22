include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O=trackDbRaFormat.o

all:  trackDbRaFormat

trackDbRaFormat: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/trackDbRaFormat ${O} $(MYLIBS) $L
	#${STRIP} ${BINDIR}/trackDbRaFormat${EXE}

clean::
	rm -f ${O}
