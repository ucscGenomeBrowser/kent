include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = wigBedToStep.o

wigBedToStep: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigBedToStep $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigBedToStep${EXE}

clean:
	rm -f $O
