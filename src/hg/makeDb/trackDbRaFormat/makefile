include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

all:  trackDbRaFormat

trackDbRaFormat: trackDbRaFormat.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/trackDbRaFormat trackDbRaFormat.o $(MYLIBS) $L
	#${STRIP} ${BINDIR}/trackDbRaFormat${EXE}


