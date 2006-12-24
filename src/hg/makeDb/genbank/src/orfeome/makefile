GBROOT = ../..
include ${GBROOT}/common.mk

O = orfeomeImageIds.o

PROGS = ${BINARCH}/orfeomeDbLoad 


all: ${PROGS}

clean:
	rm -f ${O} ${PROGS} orfeomeDbLoad.o

${BINARCH}/orfeomeDbLoad:  orfeomeDbLoad.o ${O} ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/orfeomeDbLoad orfeomeDbLoad.o ${O} $(LIBS)

