GBROOT = ../..
include ${GBROOT}/common.mk

O = orfeomeImageIds.o

PROGS = ${GB_BINARCH}/orfeomeDbLoad 


all: ${PROGS}

clean:
	rm -f ${O} ${PROGS} orfeomeDbLoad.o

${GB_BINARCH}/orfeomeDbLoad:  orfeomeDbLoad.o ${O} ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/orfeomeDbLoad orfeomeDbLoad.o ${O} $(LIBS)

