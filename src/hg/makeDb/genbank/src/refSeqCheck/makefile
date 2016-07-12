kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk

O = refSeqCheck.o
PROGS = ${GB_BINARCH}/refSeqCheck

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}

${GB_BINARCH}/refSeqCheck:  ${O} ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/refSeqCheck  ${O} $(LIBS)

